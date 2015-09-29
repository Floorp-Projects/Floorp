// Implicit copy construct which is unused
class RC1 {
  void AddRef();
  void Release();
  int mRefCnt;
};

// Explicit copy constructor which is used
class RC2 {
public:
  RC2();
  RC2(const RC2&);
private:
  void AddRef();
  void Release();
  int mRefCnt;
};

void f() {
  RC1* r1 = new RC1();
  RC1* r1p = new RC1(*r1); // expected-error {{Invalid use of compiler-provided copy constructor on refcounted type}} expected-note {{The default copy constructor also copies the default mRefCnt property, leading to reference count imbalance issues. Please provide your own copy constructor which only copies the fields which need to be copied}}

  RC2* r2 = new RC2();
  RC2* r2p = new RC2(*r2);
}
