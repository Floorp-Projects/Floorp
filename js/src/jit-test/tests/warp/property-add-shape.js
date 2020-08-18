function simple() {
  var o = {a: 1};
  o.b = 2;

  assertEq(o.a, 1);
  assertEq(o.b, 2);
}

function condition1(b) {
  var o = {a: 1};

  if (b) {
    o.b = 2;
  }

  o.c = 3;

  assertEq(o.a, 1);
  if (b) {
    assertEq(o.b, 2);
  } else {
    assertEq('b' in o, false);
  }
  assertEq(o.c, 3);
}

function condition2(b) {
  var o = {a: 1};

  if (b) {
    o.b = 2;
  } else {
    o.b = 3;
  }

  o.c = 3;

  assertEq(o.a, 1);
  assertEq(o.b, b ? 2 : 3);
  assertEq(o.c, 3);
}

function condition3(b) {
  var o = {a: 1};

  if (b) {
    o.b = 2;
  } else {
    o.b = 2;
  }

  o.c = 3;

  assertEq(o.a, 1);
  assertEq(o.b, 2);
  assertEq(o.c, 3);
}

function condition4(b) {
  var o = {a: 1};

  o.bla = 2;
  o.bla2 = 2;
  o.bla3 = 2;
  o.bla4 = 2;

  if (b) {
    o.b = 2;
  } else {
    o.c = 2;
  }

  o.d = 3;

  assertEq(o.a, 1);
  if (b) {
    assertEq(o.b, 2);
    assertEq('c' in o, false);
  } else {
    assertEq('b' in o, false);
    assertEq(o.c, 2);
  }
  assertEq(o.d, 3);
}

function f() {
  for (var i = 0; i < 10; i++) {
    simple();
    condition1(i % 2 == 0)
    condition2(i % 2 == 0)
    condition3(i % 2 == 0)
    condition4(i % 2 == 0)
  }
}

for (var i = 0; i < 10; i++) {
  f();
}
