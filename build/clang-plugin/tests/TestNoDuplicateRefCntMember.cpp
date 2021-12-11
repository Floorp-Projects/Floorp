class C1 {};

class RC1 {
public:
  virtual void AddRef();
  virtual void Release();

private:
  int mRefCnt; // expected-note 2 {{Superclass 'RC1' also has an mRefCnt member}} expected-note 3 {{Superclass 'RC1' has an mRefCnt member}}
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

class RC4 : public RC3, public C2 {}; // expected-error {{Refcounted record 'RC4' has multiple superclasses with mRefCnt members}}

class RC5 : public RC1 {};

class RC6 : public C1, public RC5 { // expected-error {{Refcounted record 'RC6' has multiple mRefCnt members}}
public:
  virtual void AddRef();
  virtual void Release();

private:
  int mRefCnt; // expected-note {{Consider using the _INHERITED macros for AddRef and Release here}}
};

class Predecl;

class OtherRC {
public:
  virtual void AddRef();
  virtual void Release();

private:
  int mRefCnt; // expected-note {{Superclass 'OtherRC' has an mRefCnt member}}
};

class MultRCSuper : public RC1, public OtherRC {}; // expected-error {{Refcounted record 'MultRCSuper' has multiple superclasses with mRefCnt members}}
