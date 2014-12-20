#define MOZ_NO_ARITHMETIC_EXPR_IN_ARGUMENT __attribute__((annotate("moz_no_arith_expr_in_arg")))

struct X {
  explicit X(int) MOZ_NO_ARITHMETIC_EXPR_IN_ARGUMENT;
  void baz(int) MOZ_NO_ARITHMETIC_EXPR_IN_ARGUMENT;
};

int operator+(int, X);
int operator+(X, int);
int operator++(X);

void badArithmeticsInArgs() {
  int a;
  typedef int myint;
  myint b;
  X goodObj1(a);
  goodObj1.baz(b);
  X badObj1(a + b); // expected-error{{cannot pass an arithmetic expression of built-in types to 'X'}}
  X badObj2 = X(a ? 0 : ++a); // expected-error{{cannot pass an arithmetic expression of built-in types to 'X'}}
  X badObj3(~a); // expected-error{{cannot pass an arithmetic expression of built-in types to 'X'}}
  badObj1.baz(a - 1 - b); // expected-error{{cannot pass an arithmetic expression of built-in types to 'baz'}}
  badObj1.baz(++a); // expected-error{{cannot pass an arithmetic expression of built-in types to 'baz'}}
  badObj1.baz(a++); // expected-error{{cannot pass an arithmetic expression of built-in types to 'baz'}}
  badObj1.baz(a || b);
  badObj1.baz(a + goodObj1);
  badObj1.baz(goodObj1 + a);
  badObj1.baz(++goodObj1);
  badObj1.baz(-1);
  badObj1.baz(-1.0);
  badObj1.baz(1 + 2);
  badObj1.baz(1 << (sizeof(int)/2));
}
