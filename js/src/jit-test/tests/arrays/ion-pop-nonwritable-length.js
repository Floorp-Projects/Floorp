load(libdir + "asserts.js");

function f(arr)
{
  arr.pop();
}

var N = 100;

function test(out)
{
  // Create an array of arrays, to be iterated over for [].pop-calling.  We
  // can't just loop on pop on a single array with non-writable length because
  // pop throws when called on an array with non-writable length.
  var arrs = out.arrs = [];
  for (var i = 0; i < N; i++)
    arrs.push([0, 1, 2, 3]);

  // Use a much-greater capacity than the eventual non-writable length.
  var a = [0, 1, 2, 3, 4, 5, 6, 7];
  Object.defineProperty(a, "length", { writable: false, value: 4 });

  arrs.push(a);

  for (var i = 0, sz = arrs.length; i < sz; i++)
    f(arrs[i]);
}

var obj = {};

assertThrowsInstanceOf(function() { test(obj); }, TypeError);

var arrs = obj.arrs;
assertEq(arrs.length, N + 1);
for (var i = 0; i < N; i++)
{
  assertEq(arrs[i].length, 3, "unexpected length for arrs[" + i + "]");
  assertEq(arrs[i][0], 0, "bad element for arrs[" + i + "][0]");
  assertEq(arrs[i][1], 1, "bad element for arrs[" + i + "][1]");
  assertEq(arrs[i][2], 2, "bad element for arrs[" + i + "][2]");
  assertEq(3 in arrs[i], false, "shouldn't be a third element");
  assertEq(arrs[i][3], undefined);
}

var a = arrs[N];
assertEq(a.hasOwnProperty(3), false, "should have been deleted before throw");
assertEq(a[3], undefined);
assertEq(a.length, 4, "length shouldn't have been changed");
