var finalizerRun = false;
var caught = false;

function foo(arr) {
  finalizerRun = true;
  return not_defined;
}

function gen() {
  try {
    yield 1;
  } finally {
    foo();
  }
}

function test() {
  var i_have_locals;
  for (i in gen()) {
    "this won't work"();
  }
}

try {
    test();
} catch(e) {
    caught = true;
    assertEq(''+e, "ReferenceError: not_defined is not defined");
}

assertEq(finalizerRun, true);
assertEq(caught, true);
