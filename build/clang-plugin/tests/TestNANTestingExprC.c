/* expected-no-diagnostics */
void test(int x);
void foo() {
  float f, f2;
  typedef double mydouble;
  mydouble d;
  double d2;
  test(f == f);
  test(d == d);
  test(f != f);
  test(d != d);
  test(f != d);
  test(d == (d - f));
  test(f == f2);
  test(d == d2);
  test(d + 1 == d);
}
