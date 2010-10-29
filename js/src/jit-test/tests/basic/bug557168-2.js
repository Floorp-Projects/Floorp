x = <x/>
try {
  Function("\
    (function f() {\
      ({x}=x);\
      f()\
    })()\
  ")()
} catch (e) {
  assertEq(e.message, "too much recursion");
}
