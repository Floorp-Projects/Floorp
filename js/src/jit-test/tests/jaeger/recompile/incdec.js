
/* Handle recompilation on overflow of inc/dec operations. */

function local()
{
  var j = 0x7ffffff0;
  for (var i = 0; i < 100; i++)
    j++;
  assertEq(j, 2147483732);
}
local();

function arg(j)
{
  for (var i = 0; i < 100; i++)
    j++;
  assertEq(j, 2147483732);
}
arg(0x7ffffff0);

var g = 0x7ffffff0;
function glob()
{
  for (var i = 0; i < 100; i++)
    g++;
  assertEq(g, 2147483732);
}
glob();

function gname()
{
  n = 0x7ffffff0;
  for (var i = 0; i < 100; i++)
    n++;
  assertEq(n, 2147483732);
}
gname();

function prop()
{
  var v = {f: 0x7ffffff0};
  for (var i = 0; i < 100; i++)
    v.f++;
  assertEq(v.f, 2147483732);
}
prop();

function elem(v, f)
{
  for (var i = 0; i < 100; i++)
    v[f]++;
  assertEq(v.f, 2147483732);
}
elem({f: 0x7ffffff0}, "f");
