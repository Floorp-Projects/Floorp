
function testEval(x, y) {
  x = 5;
  eval("arguments[0] += 10");
  assertEq(x, 15);
}
for (var i = 0; i < 5; i++)
  testEval(3);

function testEvalWithArguments(x, y) {
  eval("arguments[0] += 10");
  assertEq(arguments[y], 13);
}
for (var i = 0; i < 5; i++)
  testEvalWithArguments(3, 0);

function testNestedEval(x, y) {
  x = 5;
  eval("eval('arguments[0] += 10')");
  assertEq(x, 15);
}
for (var i = 0; i < 5; i++)
  testNestedEval(3);

function testWith(x, y) {
  with ({}) {
    arguments[0] += 10;
    assertEq(x, 13);
  }
}
for (var i = 0; i < 5; i++)
  testWith(3);
