class nsIPrincipal {
public:
  void GetURI(int foo){};
};

class SomePrincipal : public nsIPrincipal {
public:
  void GetURI(int foo) {}
};

class NullPrincipal : public SomePrincipal {};

class SomeURI {
public:
  void GetURI(int foo) {}
};

void f() {
  nsIPrincipal *a = new SomePrincipal();
  a->GetURI(0); //  expected-error {{Principal->GetURI is deprecated and will be removed soon. Please consider using the new helper functions of nsIPrincipal}}

  ::nsIPrincipal *b = new NullPrincipal();
  b->GetURI(0); //  expected-error {{Principal->GetURI is deprecated and will be removed soon. Please consider using the new helper functions of nsIPrincipal}}

  SomeURI *c = new SomeURI();
  c->GetURI(0);

  SomePrincipal *d = new SomePrincipal();
  d->GetURI(0);

}
