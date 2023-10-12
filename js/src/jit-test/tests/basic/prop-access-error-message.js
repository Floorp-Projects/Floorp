// |jit-test| skip-if: getBuildConfiguration('pbl')
// The decompiled expression used in the error messsage should not be confused
// by unrelated value on the stack.

var a = {};
var b = {};
var p = "c";

function doPropAccess() {
  // Both "a.c" and "b.e" are undefined.
  // "a.c" should be used.
  a.c.d = b.e;
}

function testPropAccess() {
  var caught = false;
  try {
    doPropAccess();
  } catch (e) {
    assertEq(e.message.includes("a.c is undefined"), true);
    caught = true;
  }
  assertEq(caught, true);
}

function doElemAccess() {
  // Both "a[x]" and "b.e" are undefined.
  // "a[x]" should be used.
  var x = "c";
  a[x].d = b.e;
}

function testElemAccess() {
  var caught = false;
  try {
    doElemAccess();
  } catch (e) {
    assertEq(e.message.includes("a[x] is undefined"), true);
    caught = true;
  }
  assertEq(caught, true);
}

for (var i = 0; i < 10; i++) {
  testPropAccess();
  testElemAccess();
}
