function f() {
  eval("g=function() { \
          for (let x=0; x < 2; ++x) { \
            d=x \
          } \
        }")
  g();
  eval("var d")
  g();
}

f();
assertEq(d, 1);
