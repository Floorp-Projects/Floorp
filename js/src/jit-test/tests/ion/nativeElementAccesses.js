
function testOverwritingSparseHole()
{
  var x = [];
  for (var i = 0; i < 50; i++)
    x[i] = i;
  var hit = false;
  Object.defineProperty(x, 40, {set: function() { hit = true; }});
  for (var i = 0; i < 50; i++)
    x[i] = 10;
  assertEq(hit, true);
}
testOverwritingSparseHole();

function testReadingSparseHole()
{
  var x = [];
  for (var i = 5; i < 50; i++)
    x[i] = i;
  var hit = false;
  Object.defineProperty(x, 40, {get: function() { hit = true; return 5.5; }});
  var res = 0;
  for (var i = 0; i < 50; i++) {
    res += x[i];
    if (i == 10)
      res = 0;
  }
  assertEq(res, 1135.5);
  assertEq(hit, true);
}
testReadingSparseHole();

function testInSparseHole()
{
  var x = [];
  for (var i = 5; i < 50; i++)
    x[i] = i;
  Object.defineProperty(x, 40, {get: function() {}});
  var res = 0;
  for (var i = 0; i < 50; i++)
    res += (i in x) ? 1 : 0;
  assertEq(res, 45);
}
testInSparseHole();
