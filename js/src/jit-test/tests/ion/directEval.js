
// test scope iteration when throwing from within an eval.
function testEvalThrow(x, y) {
  x = 5;
  for (var i in [1,2,3])
    eval("x += 5; if (i == 2) throw 0");
  assertEq(x, 10);
}
for (var i = 0; i < 5; i++)
  try { testEvalThrow.call({}, 3); } catch (e) { assertEq(e, 0); }
