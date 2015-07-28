class C1 {};

class RC1 {
public:
  virtual void AddRef();
  virtual void Release();

private:
  int mRefCnt; // expected-note 2 {{Superclass 'RC1' also has an mRefCnt member}}
};

class RC2 : public RC1 { // expected-error {{Refcounted record 'RC2' has multiple mRefCnt members}}
public:
  virtual void AddRef();
  virtual void Release();

private:
  int mRefCnt; // expected-note {{Consider using the _INHERITED macros for AddRef and Release here}}
};

class C2 : public RC1 {};

class RC3 : public RC1 {};

class RC4 : public C2, public RC1 {};

class RC5 : public RC1 {};

class RC6 : public C1, public RC5 { // expected-error {{Refcounted record 'RC6' has multiple mRefCnt members}}
public:
  virtual void AddRef();
  virtual void Release();

private:
  int mRefCnt; // expected-note {{Consider using the _INHERITED macros for AddRef and Release here}}
};

class Predecl;
