

function testEvalThrow(x, y) {
  eval("");
}
for (var i = 0; i < 5; i++)
  testEvalThrow.call("");
