function neg0(x) {
  return x===0 && (1/x)===-Infinity;
}

function test(x,y) {
  if (x == 1.1)
    return 0;
  else if (x == "a")
    return 0;
  return x*y
}

var t = 0;
for(var i=0; i<1005; i++) {
  test(1.1)
  test("a")
  t = test((i<1003)?i:-0, 0);
}

assertEq(neg0(t), true);
