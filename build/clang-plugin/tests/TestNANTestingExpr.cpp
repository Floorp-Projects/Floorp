#line 1 "tests/SkScalar.h"
// This checks that the whitelist accounts for #line directives and such. If you
// remove SkScalar from the whitelist, please change the filename here instead
// of adding expected diagnostics.
inline int headerSays(double x) {
  return x != x;
}
#line 9 "TestNANTestingExpr.cpp"
void test(bool x);
void foo() {
  float f, f2;
  typedef double mydouble;
  mydouble d;
  double d2;
  test(f == f); // expected-error{{comparing a floating point value to itself for NaN checking can lead to incorrect results}} expected-note{{consider using std::isnan instead}}
  test(d == d); // expected-error{{comparing a floating point value to itself for NaN checking can lead to incorrect results}} expected-note{{consider using std::isnan instead}}
  test(f != f); // expected-error{{comparing a floating point value to itself for NaN checking can lead to incorrect results}} expected-note{{consider using std::isnan instead}}
  test(d != d); // expected-error{{comparing a floating point value to itself for NaN checking can lead to incorrect results}} expected-note{{consider using std::isnan instead}}
  test(f != d);
  test(d == (d - f));
  test(f == f2);
  test(d == d2);
  test(d + 1 == d);
}
