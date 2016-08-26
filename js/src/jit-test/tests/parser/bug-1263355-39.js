function f() {
  var init, first;
  for (let i = (init = () => i = 1, 0); (first = () => i, i) < 0; ++i);
  assertEq(init(), 1);
  assertEq(first(), 0);
}

f();
