function bar(x) {
  with ({}) {}
  switch (x) {
  case 1:
    foo(2);
    break;
  case 2:
    gczeal(14, 1);
    break;
  }
  return "a sufficiently long string";
}

function foo(x) {
  for (var s in bar(x)) { gczeal(0); }
}

with ({}) {}
for (var i = 0; i < 100; i++) {
  foo(0);
}
foo(1);
