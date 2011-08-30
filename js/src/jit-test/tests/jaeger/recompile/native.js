
/* Handle recompilations triggered by native calls. */

function dofloor(v)
{
  var res = "";
  for (var i = 0; i < 10; i++) {
    var q = Math.floor(v + i);
    res += q + ",";
  }
  assertEq(res, "2147483642,2147483643,2147483644,2147483645,2147483646,2147483647,2147483648,2147483649,2147483650,2147483651,");
}
dofloor(0x7ffffffa + .5);

function mapfloor(a)
{
  var b = a.map(function(v) { return Math.floor(v); });

  var res = "";
  for (var i = 0; i < b.length; i++)
    res += b[i] + ",";
  return res;
}
mapfloor([1,2]);
mapfloor([3,4]);
assertEq(mapfloor([0x7ffffffa + 2.5, 0x7ffffffa + 20.5]), "2147483644,2147483662,");
