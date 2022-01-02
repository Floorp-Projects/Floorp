function g(a, b, c, d) {}
function f(a, b, c)
{
  arguments.length = getMaxArgs() + 1;
  g.apply(this, arr);
}
let x = [];
x.length = getMaxArgs() + 1;
var args = [[5], [5], [5], [5], [5], [5], [5], [5], [5], [5], [5], x]
try
{
  for (var i = 0; i < args.length; i++) { arr = args[i]; f(); }
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e instanceof RangeError, true, "wrong exception: " + e);
}
try
{
  for (var i = 0; i < args.length; i++) { arr = args[i]; f(); }
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e instanceof RangeError, true, "wrong exception: " + e);
}
