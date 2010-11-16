
/* Handle recompilation on undefined properties and array holes. */

var v = {};
if (0)
  v.x = 0;
function prop(x)
{
  assertEq(prop.x, undefined);
}
prop(v);

v = [];
v[0] = 0;
v[1] = 1;
v[3] = 3;
v[4] = 4;
function elem(x)
{
  var x = "";
  for (var i = 0; i < 5; i++)
    x += v[i];
  assertEq(x, "01undefined34");
}
elem(v);
