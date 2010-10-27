x = <x/>
try {
  Function("\
    (function f() {\
      ({x:{b}}=x);\
      f()\
    })()\
  ")()
} catch (e) {
  assertEq(e.message, "too much recursion");
}
