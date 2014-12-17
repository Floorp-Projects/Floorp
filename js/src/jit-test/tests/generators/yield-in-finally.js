// return value in try block should not be overridden by yield in finally block.

load(libdir + "asserts.js");

// simple
function* g1() {
  try {
    return 42;
  } finally {
    yield 43;
  }
}
var o = g1();
var v = o.next();
assertEq(v.done, false);
assertEq(v.value, 43);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, 42);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, undefined);

// without return value
function* g2() {
  try {
    return;
  } finally {
    yield 43;
  }
}
o = g2();
v = o.next();
assertEq(v.done, false);
assertEq(v.value, 43);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, undefined);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, undefined);

// nested try-finally
function* g3() {
  try {
    try {
      return 42;
    } finally {
      try {
        return 43;
      } finally {
        yield 44;
      }
    }
  } finally {
    yield 45;
  }
}
o = g3();
v = o.next();
assertEq(v.done, false);
assertEq(v.value, 44);
v = o.next();
assertEq(v.done, false);
assertEq(v.value, 45);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, 43);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, undefined);

// yield*
function* g4() {
  try {
    return 42;
  } finally {
    try {
      return 43;
    } finally {
      yield* g5();
    }
  }
}
function* g5() {
  yield 44;
  return 45;
}
o = g4();
v = o.next();
assertEq(v.done, false);
assertEq(v.value, 44);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, 43);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, undefined);

// return in block scope
function* g6() {
  let a = 10;
  {
    let a = 20;
    try {
      let a = 30;
      {
        let a = 40;
        return 42;
      }
    } finally {
      yield 43;
    }
  }
}
o = g6();
v = o.next();
assertEq(v.done, false);
assertEq(v.value, 43);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, 42);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, undefined);

// no finally
function* g7() {
  try {
    return 42;
  } catch (e) {
    yield 1;
  }
}
o = g7();
v = o.next();
assertEq(v.done, true);
assertEq(v.value, 42);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, undefined);

// legacy iterator have no return value
function g8() {
  try {
    return;
  } finally {
    yield 43;
  }
}
o = g8();
v = o.next();
assertEq(v, 43);
assertThrowsInstanceOf(() => o.next(), StopIteration);

// in "with" statement
options("strict");
eval(`
function* g9() {
  with ({ ".genrval": { value: 44, done: false } }) {
    try {
      return 42;
    } finally {
      yield 43;
    }
  }
}
o = g9();
v = o.next();
assertEq(v.done, false);
assertEq(v.value, 43);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, 42);
v = o.next();
assertEq(v.done, true);
assertEq(v.value, undefined);
`);
