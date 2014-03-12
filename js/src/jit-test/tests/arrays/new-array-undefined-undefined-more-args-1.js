for (var i = 0; i < 1e4; i++)
{
  assertEq(typeof new Array(undefined, undefined, 1, 2, 3, 4).sort()[0],
           "number",
           "bad, i: " + i);
}
