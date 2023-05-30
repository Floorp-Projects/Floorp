function test() {
  let count = 0;
  for (var i = 0; i < 30; i++) {
    assertEq(arguments.callee, i > 20 ? 3 : test);
    if (i === 20) {
      Object.defineProperty(arguments, "callee", {get: function() { count++; return 3; }});
    }
  }
  assertEq(count, 9);
}
test();

function testUnusedResult() {
  let count = 0;
  for (var i = 0; i < 30; i++) {
    arguments.callee;
    if (i === 20) {
      Object.defineProperty(arguments, "callee", {get: function() { count++; return 3; }});
    }
  }
  assertEq(count, 9);
}
testUnusedResult();

function testSetter() {
  Object.defineProperty(arguments, "callee", {set: function() {}});
  for (var i = 0; i < 15; i++) {
    assertEq(arguments.callee, undefined);
  }
}
testSetter();
