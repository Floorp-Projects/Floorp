void test(bool x);
void foo() {
  float f, f2;
  typedef double mydouble;
  mydouble d;
  double d2;
  test(f == f); // expected-error{{comparing a floating point value to itself for NaN checking can lead to incorrect results}} expected-note{{consider using mozilla::IsNaN instead}}
  test(d == d); // expected-error{{comparing a floating point value to itself for NaN checking can lead to incorrect results}} expected-note{{consider using mozilla::IsNaN instead}}
  test(f != f); // expected-error{{comparing a floating point value to itself for NaN checking can lead to incorrect results}} expected-note{{consider using mozilla::IsNaN instead}}
  test(d != d); // expected-error{{comparing a floating point value to itself for NaN checking can lead to incorrect results}} expected-note{{consider using mozilla::IsNaN instead}}
  test(f != d);
  test(d == (d - f));
  test(f == f2);
  test(d == d2);
  test(d + 1 == d);
}
