load(libdir + 'asserts.js');
load(libdir + 'iteration.js');

var outer = "unmodified";

function f(v)
{
  if (v + "")
    ({ [(outer = "modified")]: v } = v);
}

assertEq(outer, "unmodified");
f(true);
assertEq(outer, "modified");

outer = "unmodified";
f({});
assertEq(outer, "modified");

outer = "unmodified";
assertThrowsInstanceOf(() => f(null), TypeError);
assertEq(outer, "unmodified");

assertThrowsInstanceOf(() => f(undefined), TypeError);
assertEq(outer, "unmodified");


function g(v)
{
  if (v + "")
    ({ [{ toString() { outer = "modified"; return 0; } }]: v } = v);
}

outer = "unmodified";
g(true);
assertEq(outer, "modified");

outer = "unmodified";
g({});
assertEq(outer, "modified");

outer = "unmodified";
assertThrowsInstanceOf(() => g(undefined), TypeError);
assertEq(outer, "unmodified");

assertThrowsInstanceOf(() => g(null), TypeError);
assertEq(outer, "unmodified");
