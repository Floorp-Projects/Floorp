for (var i = 0; i < 1e4; i++)
{
  assertEq(new Array(undefined, undefined, 1, 2, 3, 4).length,
           6,
           "bad, i: " + i);
}
