x = <x/>
try {
  Function("\
    (function f() {\
      ({x:{b}}=x);\
      f.apply(null, new Array(100))\
    })()\
  ")()
} catch (e) {
  assertEq(e.message, "too much recursion");
}
