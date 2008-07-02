function test(desc, expected, actual)
{
  if (expected == actual)
    return print(desc, ": passed");
  print(desc, ": FAILED: ", typeof(expected), "(", expected, ") != ",
	typeof(actual), "(", actual, ")");
}

function ifInsideLoop()
{
  var cond = true, count = 0;
  for (var i = 0; i < 5000; i++) {
    if (cond)
      count++;
  }
  return count;
}
test("tracing if", ifInsideLoop(), 5000);

function bitwiseAnd(bitwiseAndValue) {
  for (var i = 0; i < 60000000; i++)
    bitwiseAndValue = bitwiseAndValue & i;
  return bitwiseAndValue;
}
test("bitwise and with arg/var", bitwiseAnd(12341234), 0)

function equalInt()
{
  var i1 = 55, eq = 0;
  for (var i = 0; i < 5000; i++) {
    if (i1 == 55)
      eq++;
  }
  return eq;
}
test("int equality", equalInt(), 5000);