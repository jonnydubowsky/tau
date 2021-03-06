// LICENSE
// This software is free for use and redistribution while including this license notice, unless:
// 1. is used for commercial or non-personal purposes, or
// 2. used for a product which includes or associated with a blockchain or other decentralized database technology, or
// 3. used for a product which includes or associated with the issuance or use of cryptographic or electronic currencies/coins/tokens.
// On all of the mentioned cases, an explicit and written permission is required from the Author (Ohad Asor).
// Contact ohad@idni.org for requesting a permission.  This license may be modified over time by the Author.
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>
#include <climits>
#include <stdexcept>
using namespace std;

typedef int32_t int_t;
typedef array<int_t, 3> node; // [bdd] node is a triple: varid, 1-node-id, 0-node-id
typedef const wchar_t* wstr;
template<typename K> using matrix = vector<vector<K>>; // used as a set of terms (e.g. rule)
typedef vector<bool> bools;
typedef vector<bools> vbools;
#define er(x)	perror(x), exit(0)
#define oparen_expected "'(' expected\n"
#define comma_expected "',' or ')' expected\n"
#define dot_after_q "expected '.' after query.\n"
#define if_expected "'if' or '.' expected\n"
#define sep_expected "Term or ':-' or '.' expected\n"
#define unmatched_quotes "Unmatched \"\n"
#define err_inrel "Unable to read the input relation symbol.\n"
#define err_src "Unable to read src file.\n"
#define err_dst "Unable to read dst file.\n"
////////////////////////////////////////////////////////////////////////////////////////////////////
struct rule { // a [P-DATALOG] rule in bdd form with additional information
	bool neg;
	int_t h, hsym; // bdd root and head syms
	size_t w; // nbodies, will determine the virtual power
	set<int_t> x; // existentials
	map<int_t, int_t> hvars; // how to permute body vars to head vars
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class bdds_base {
	vector<node> V; // all nodes
	map<node, int_t> M; // node to its index
	int_t root; // used for implicit power
	size_t dim = 1; // used for implicit power
protected:
	int_t add_nocheck(const node& n) {
		V.emplace_back(n);
		int_t r = (M[n]=V.size()-1);
		return r;
	}
	bdds_base() { add_nocheck({{0, 0, 0}}), add_nocheck({{0, 1, 1}}); }
public:
	static const int_t F = 0, T = 1;
	node getnode(size_t n) const; // node from id. equivalent to V[n] unless virtual pow is used
	int_t setpow(int_t _root, size_t _dim) { root=_root, dim=_dim; return leaf(root)?root:(root+V.size()*(dim-1));}
	static bool leaf(int_t x) { return x == T || x == F; }
	static bool leaf(const node& x) { return !x[0]; }
	static bool trueleaf(const node& x) { return leaf(x) && x[1]; }
	static bool trueleaf(const int_t& x) { return x == T; }
	wostream& out(wostream& os, const node& n) const; // print a bdd using ?: syntax
	wostream& out(wostream& os, size_t n) const	{ return out(os, getnode(n)); }
	int_t add(const node& n);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// the following to be used with bdds::apply()
struct op_or_t { int_t operator()(int_t x, int_t y) const { return (x||y)?bdds_base::T:bdds_base::F; } } op_or; 
struct op_and_t { int_t operator()(int_t x, int_t y) const { return (x&&y)?bdds_base::T:bdds_base::F; } } op_and; 
struct op_and_not_t { int_t operator()(int_t x, int_t y) const { return (x&&!y)?bdds_base::T:bdds_base::F; } } op_and_not;
////////////////////////////////////////////////////////////////////////////////////////////////////
vbools& operator*=(vbools& x, const pair<const vbools&, size_t>& y); // to be used with allsat()

class bdds : public bdds_base { // holding functions only, therefore tbd: dont use it as an object
	void sat(int_t v, int_t nvars, node n, bools& p, vbools& r) const;
public:
	int_t from_bit(int_t x, bool v) { return add(v ? node{{x+1, T, F}} : node{{x+1, F, T}}); }
	int_t ite(int_t v, int_t t, int_t e);
	template<typename op_t> static // binary application
	int_t apply(const bdds& bx, int_t x, const bdds& by, int_t y, bdds& r, const op_t& op);
	template<typename op_t> static int_t apply(const bdds& b, int_t x, bdds& r, const op_t& op);
	template<typename op_t> static int_t apply(bdds& b, int_t x,bdds& r, const op_t& op);//unary
	static int_t permute(bdds& b, int_t x, bdds& r, const map<int_t, int_t>&);
	// helper constructors
	int_t from_eq(int_t x, int_t y); // a bdd saying "x=y"
	template<typename K> rule from_rule(matrix<K> v, const size_t bits, const size_t ar);
	template<typename K> matrix<K> from_bits(int_t x, const size_t bits, const size_t ar);
	// helper apply() variations
	int_t bdd_or(int_t x, int_t y)	{ return apply(*this, x, *this, y, *this, op_or); } 
	int_t bdd_and(int_t x, int_t y)	{ return apply(*this, x, *this, y, *this, op_and); } 
	int_t bdd_and_not(int_t x, int_t y){ return apply(*this, x, *this, y, *this, op_and_not); }
	vbools allsat(int_t x, size_t nvars) const;
	using bdds_base::add;
	using bdds_base::out;
};

struct op_exists { // existential quantification, to be used with apply()
	const set<int_t>& s;
	op_exists(const set<int_t>& s) : s(s) { }
	node operator()(bdds& b, const node& n) const {
		if (!n[0]) return n;
		return s.find(n[0]-1) == s.end() ? n : b.getnode(b.bdd_or(n[1], n[2]));
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename K> class dict_t { // handles representation of strings as unique integers
	struct dictcmp {
		bool operator()(const pair<wstr, size_t>& x, const pair<wstr, size_t>& y) const;
	};
	map<pair<wstr, size_t>, K, dictcmp> syms_dict, vars_dict;
	vector<wstr> syms;
	vector<size_t> lens;
public:
	const K pad = 0;
	dict_t() { syms.push_back(0), lens.push_back(0), syms_dict[{0, 0}] = pad; }
	K operator()(wstr s, size_t len);
	pair<wstr, size_t> operator()(K t) const { return { syms[t], lens[t] }; }
	size_t bits() const { return (sizeof(K)<<3) - __builtin_clz(syms.size()); }
	size_t nsyms() const { return syms.size(); }
};
wostream& operator<<(wostream& os, const pair<wstr, size_t>& p) {
	for (size_t n = 0; n < p.second; ++n) os << p.first[n];
	return os;
}
template<typename K>
wostream& out(wostream& os, bdds& b, int_t db, size_t bits, size_t ar, const class dict_t<K>& d);
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename K> class lp { // [pfp] logic program
	dict_t<K> dict; // hold its own dict so we can determine the universe size
	bdds prog, dbs; // separate bdds for prog and db cause db is a virtual power
	vector<rule> rules; // prog's rules

	K str_read(wstr *s); // parser's helper, reads a string and returns its dict id
	vector<K> term_read(wstr *s); // read raw term (no bdd)
	matrix<K> rule_read(wstr *s); // read raw rule (no bdd)
	size_t bits, ar;
public:
	int_t db; // db's bdd root
	void prog_read(wstr s);
	void step(); // single pfp step
	bool pfp();
	void printdb(wostream& os) { out<K>(os, dbs, db, bits, ar, dict); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
wostream& operator<<(wostream& os, const bools& x) { for (auto y:x) os << (y?'1':'0');return os; }
wostream& operator<<(wostream& os, const vbools& x) { for (auto y:x) os << y << endl; return os; }
template<typename K> wostream& out(wostream& os, bdds& b, int_t db, size_t bits, size_t ar, const dict_t<K>& d) {
	for (auto v : b.from_bits<K>(db, bits, ar)) {
		for (auto k : v)
			if (!k) os << L"* ";
			else if (k < (int_t)d.nsyms()) os << d(k) << L' ';
			else os << L'[' << k << L"] ";
		os << endl;
	}
	return os;
}

int_t bdds_base::add(const node& n) { // create new bdd node, standard implementation
	if (n[1] == n[2]) return n[1];
	auto it = M.find(n);
	return it == M.end() ? add_nocheck(n) : it->second;
}

node bdds_base::getnode(size_t n) const { // returns a bdd node considering virtual powers
	if (dim == 1 || n < V.size()) return V[n];
	const size_t m = n % V.size(), d = n / V.size();
	node r = V[m];
	if (trueleaf(r[1])) r[1] = root + V.size() * (d - 1);
	else if (!leaf(r[1])) r[1] += V.size() * d;
	if (trueleaf(r[2])) r[2] = root + V.size() * (d - 1);
	else if (!leaf(r[2])) r[2] += V.size() * d;
	return r;
}

void bdds::sat(int_t v, int_t nvars, node n, bools& p, vbools& r) const {
	if (leaf(n) && !trueleaf(n)) return;
	if (v<n[0]) p[v-1] = true, sat(v+1, nvars, n, p, r), p[v-1]=false, sat(v+1, nvars, n, p, r);
	else if (v == nvars+1) r.push_back(p);
	else p[v-1]=true, sat(v+1,nvars,getnode(n[1]),p,r), p[v-1]=false, sat(v+1,nvars,getnode(n[2]),p,r);
}

vbools bdds::allsat(int_t x, size_t nvars) const {
	bools p;
	p.resize(nvars);
	vbools r;
	return sat(1, nvars, getnode(x), p, r), r;
}

int_t bdds::from_eq(int_t x, int_t y) {
	return bdd_or(	bdd_and(from_bit(x, true), from_bit(y, true)),
			bdd_and(from_bit(x, false),from_bit(y, false)));
}

wostream& bdds_base::out(wostream& os, const node& n) const {
	if (leaf(n)) return os << (trueleaf(n) ? L'T' : L'F');
	else return out(os << n[0] << L'?', getnode(n[1])), out(os << L':', getnode(n[2]));
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename op_t> int_t bdds::apply(const bdds& b, int_t x, bdds& r, const op_t& op) { //unary
	node n = op(b, b.getnode(x));
	return r.add({n[0], leaf(n[1])?n[1]:apply(b,n[1],r,op), leaf(n[2])?n[2]:apply(b,n[2],r,op)});
}

template<typename op_t> int_t bdds::apply(bdds& b, int_t x, bdds& r, const op_t& op) { // nonconst
	node n = op(b, b.getnode(x));
	return r.add({{n[0], leaf(n[1])?n[1]:apply(b,n[1],r,op), leaf(n[2])?n[2]:apply(b,n[2],r,op)}});
}

int_t bdds::ite(int_t v, int_t t, int_t e) {
	return bdd_or(bdd_and(from_bit(v, true), t), bdd_and(from_bit(v, false), e));
}

// [overlapping] rename
int_t bdds::permute(bdds& b, int_t x, bdds& r, const map<int_t, int_t>& m) {
	node n = b.getnode(x);
	if (leaf(n)) return x;
	auto it = m.find(n[0]-1);
	return r.ite(it==m.end() ? n[0]-1 : it->second, permute(b,n[1],r,m), permute(b,n[2],r,m));
}

template<typename op_t> // binary application
int_t bdds::apply(const bdds& bx, int_t x, const bdds& by, int_t y, bdds& r, const op_t& op) {
	const node &Vx = bx.getnode(x), &Vy = by.getnode(y);
	const int_t &vx = Vx[0], &vy = Vy[0];
	int_t v, a = Vx[1], b = Vy[1], c = Vx[2], d = Vy[2];
	if ((!vx && vy) || (vy && (vx > vy))) a = c = x, v = vy;
	else if (!vx) return op(a, b);
	else if ((v = vx) < vy || !vy) b = d = y;
	return r.add({{v, apply(bx, a, by, b, r, op), apply(bx, c, by, d, r, op)}});
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename K> vector<K> from_bits(const bools& x, const size_t ar) {
	vector<K> r(ar, 0);
	for (size_t n = 0; n < x.size(); ++n) if (x[n]) r[n % ar] |= 1<<(n / ar);
	return r;
}
template<typename K> matrix<K> bdds::from_bits(int_t x, const size_t bits, const size_t ar) {
	vbools s = allsat(x, bits * ar);
	matrix<K> r(s.size());
	size_t n = 0;
	for (const bools& b : s) r[n++] = ::from_bits<K>(b, ar);
	return r;
}
template<typename K> rule bdds::from_rule(matrix<K> v, const size_t bits, const size_t ar) {
	int_t k, notpad, npad = F;
	size_t i, j, b;
	map<K, int_t> hvars;
	vector<K>& head = v[v.size()-1];
	bool bneg;
	rule r;
	r.h = r.hsym = T;
	r.neg = head[0] < 0;
	head.erase(head.begin());
	r.w = v.size() - 1;
	#define BIT(term,arg) (term*bits+b)*ar+arg
	for (i = 0; i != head.size(); ++i)
		if (head[i] < 0) hvars.emplace(head[i], i); // var
		else for (b = 0; b != bits; ++b) r.hsym = bdd_and(r.hsym, from_bit(BIT(0, i), head[i]&(1<<b)));
	map<K, array<size_t, 2>> m;
	if (v.size()==1) r.h = r.hsym;
	else for (i = 0; i != v.size()-1; ++i, r.h = bneg ? bdd_and_not(r.h, k) : bdd_and(r.h, k))
		for (k=T, bneg = (v[i][0]<0), v[i].erase(v[i].begin()), j=0; j != v[i].size(); ++j)
			if (auto it = m.find(v[i][j]); it != m.end()) { // if seen
				for (b=0; b!=bits; ++b)	k = bdd_and(k,from_eq(BIT(i,j),
								BIT(it->second[0], it->second[1])));
				if (hvars.find(v[i][j]) != hvars.end()) // existential out if headvar
					for (b=0; b!=bits; ++b) r.x.emplace(BIT(i,j));
			} else if (m.emplace(v[i][j], array<size_t, 2>{ {i, j} }); v[i][j] >= 0) // sym
				for (b=0; b!=bits; ++b)
					k = bdd_and(k, from_bit(BIT(i,j), v[i][j]&(1<<b))),
					r.x.emplace(BIT(i,j));
			else {
				for (b=0, notpad = T; b!=bits; ++b)
					notpad = bdd_and(notpad, from_bit(BIT(i, j), false));
				npad = bdd_or(npad, notpad);
				if (auto jt = hvars.find(v[i][j]); jt == hvars.end()) //non-head var
					for (b=0; b!=bits; ++b) r.x.emplace(BIT(i,j));
				else for (b=0; b!=bits; ++b)
					r.hvars.emplace(BIT(i,j), BIT(0, jt->second));
			}
	#undef BIT
	r.h = bdd_and_not(r.h, npad);
	//out(wcout<<"from_rule: ", r.h)<<endl;
	return r;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename K>
bool dict_t<K>::dictcmp::operator()(const pair<wstr, size_t>& x, const pair<wstr, size_t>& y) const{
	if (x.second != y.second) return x.second < y.second;
	return wcsncmp(x.first, y.first, x.second) < 0;
}

template<typename K> K dict_t<K>::operator()(wstr s, size_t len) {
	if (*s == L'?') {
		auto it = vars_dict.find({s, len});
		if (it != vars_dict.end()) return it->second;
		K r = -vars_dict.size() - 1;
		return vars_dict[{s, len}] = r;
	}
	auto it = syms_dict.find({s, len});
	if (it != syms_dict.end()) return it->second;
	return syms.push_back(s), lens.push_back(len), syms_dict[{s,len}] = syms.size() - 1;
}

template<typename K> K lp<K>::str_read(wstr *s) {
	wstr t;
	while (**s && iswspace(**s)) ++*s;
	if (!**s) throw runtime_error("identifier expected");
	if (*(t = *s) == L'?') ++t;
	while (iswalnum(*t)) ++t;
	if (t == *s) throw runtime_error("identifier expected");
	K r = dict(*s, t - *s);
	while (*t && iswspace(*t)) ++t;
	return *s = t, r;
}

template<typename K> vector<K> lp<K>::term_read(wstr *s) {
	vector<K> r;
	while (iswspace(**s)) ++*s;
	if (!**s) return r;
	bool b = **s == L'~';
	if (b) ++*s, r.push_back(-1); else r.push_back(1);
	do {
		while (iswspace(**s)) ++*s;
		if (**s == L',') return ++*s, r;
		if (**s == L'.' || **s == L':') return r;
		r.push_back(str_read(s));
	} while (**s);
	er("term_read(): unexpected parsing error");
}

template<typename K> matrix<K> lp<K>::rule_read(wstr *s) {
	vector<K> t;
	matrix<K> r;
	if ((t = term_read(s)).empty()) return r;
	r.push_back(t);
	while (iswspace(**s)) ++*s;
	if (**s == L'.') return ++*s, r;
	while (iswspace(**s)) ++*s;
	if (*((*s)++) != L':' || *((*s)++) != L'-') er(sep_expected);
loop:	if ((t = term_read(s)).empty()) er("term expected");
	r.insert(r.end()-1, t); // make sure head is last
	while (iswspace(**s)) ++*s;
	if (**s == L'.') return ++*s, r;
	goto loop;
}

template<typename K> void lp<K>::prog_read(wstr s) {
	vector<matrix<K>> r;
	db = bdds::F;
	size_t l;
	ar = 0;
	for (matrix<K> t; !(t = rule_read(&s)).empty(); r.push_back(t))
		for (const vector<K>& x : t) // we really support a single rel arity
			ar = max(ar, x.size()-1); // so we'll pad everything
	for (matrix<K>& x : r)
		for (vector<K>& y : x)
			if ((l=y.size()) < ar+1)
				y.resize(ar+1), fill(y.begin()+l, y.end(), dict.pad); // the padding
	bits = dict.bits();
	for (const matrix<K>& x : r)
	 	if (x.size() == 1) db = dbs.bdd_or(db, dbs.from_rule(x, bits, ar).h);// fact
		else rules.push_back(prog.from_rule(x, bits, ar)); // rule
}

template<typename K> void lp<K>::step() {
	int_t add = bdds::F, del = bdds::F, s, x, y, z;
	wcout << endl;
	for (const rule& r : rules) { // per rule
		int_t root = dbs.setpow(db, r.w);
		out<K>(wcout<<endl<<"rule: ", prog, r.h, bits, ar, dict)<<endl;
		x = bdds::leaf(db) ? bdds::trueleaf(db) ? r.h : bdds_base::F : bdds::apply(dbs, root, prog, r.h, prog, op_and); // rule/db conjunction
		prog.out(wcout<<"x: ", x)<<endl;
		out<K>(wcout<<"x: ", prog, x, bits, ar, dict)<<endl;
		y = bdds::apply(prog, x, prog, op_exists(r.x)); // remove nonhead variables
		out<K>(wcout<<"y: ", prog, y, bits, ar, dict)<<endl;
		z = bdds::permute(prog, y, prog, r.hvars); // reorder the remaining vars
		out<K>(wcout<<"z: ", prog, z, bits, ar, dict)<<endl;
		out<K>(wcout<<"hsym: ", prog, r.hsym, bits, ar, dict)<<endl;
		z = prog.bdd_and(z, r.hsym);
		out<K>(wcout<<"z&hsym: ", prog, z, bits, ar, dict)<<endl;
		(r.neg ? del : add) = bdds::apply(dbs, r.neg ? del : add, prog, z, dbs, op_or);
	}
	dbs.out(wcout<<"db: ", db)<<endl;
	dbs.out(wcout<<"add: ", add)<<endl;
	dbs.out(wcout<<"del: ", del)<<endl;
	if ((s = dbs.bdd_and_not(add, del)) == bdds::F && add != bdds::F) db = bdds::F; // detect contradiction
	else db = dbs.bdd_or(dbs.bdd_and_not(db, del), s);// db = (db|add)&~del = db&~del | add&~del
	dbs.out(wcout<<"db: ", db)<<endl;
}

template<typename K> bool lp<K>::pfp() {
	int_t d, t = 0;
	for (set<int_t> s;;) {
		s.emplace(d = db);
		printdb(wcout<<"step: " << ++t << endl);
		step();
		//printdb(wcout<<"after step: " << t << endl);
		if (s.find(db) != s.end()) return d == db;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
wstring file_read_text(FILE *f) {
	wstringstream ss;
	wchar_t buf[32], n, l, skip = 0;
	wint_t c;
	*buf = 0;
next:	for (n = l = 0; n != 31; ++n)
		if (WEOF == (c = getwc(f))) { skip = 0; break; }
		else if (c == L'#') skip = 1;
		else if (c == L'\r' || c == L'\n') skip = 0, buf[l++] = c;
		else if (!skip) buf[l++] = c;
	if (n) {
		buf[l] = 0, ss << buf;
		goto next;
	} else if (skip) goto next;
	return ss.str();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int main() {
	setlocale(LC_ALL, "");
	lp<int32_t> p;
	wstring s = file_read_text(stdin); // got to stay in memory
	p.prog_read(s.c_str());
	if (!p.pfp()) wcout << "unsat" << endl;
	return 0;
}
