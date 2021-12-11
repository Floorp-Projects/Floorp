load(libdir + "asserts.js");

function f(arr)
{
  assertEq(arr.splice(1, 2, 9, 8, 7, 6).length, 2); // if it doesn't throw :-)
}

var N = 100;

function test(out)
{
  // Create an array of arrays, to be iterated over for [].splice-calling.
  var arrs = out.arrs = [];
  for (var i = 0; i < N; i++)
    arrs.push([0, 1, 2, 3]);

  // Use a much-greater capacity than the eventual non-writable length, just for
  // variability.
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
  assertEq(arrs[i].length, 6, "unexpected length for arrs[" + i + "]");
  assertEq(arrs[i][0], 0,  "bad element for arrs[" + i + "][0]");
  assertEq(arrs[i][1], 9, "bad element for arrs[" + i + "][1]");
  assertEq(arrs[i][2], 8, "bad element for arrs[" + i + "][2]");
  assertEq(arrs[i][3], 7, "bad element for arrs[" + i + "][3]");
  assertEq(arrs[i][4], 6, "bad element for arrs[" + i + "][4]");
  assertEq(arrs[i][5], 3, "bad element for arrs[" + i + "][5]");
}

var a = arrs[N];
assertEq(a[0], 0, "bad element for a[0]");
assertEq(a[1], 1, "bad element for a[1]");
assertEq(a[2], 2, "bad element for a[2]");
assertEq(a[3], 3, "bad element for a[3]");
assertEq(a.hasOwnProperty(4), false, "shouldn't have added any elements");
assertEq(a[4], undefined);
assertEq(a.hasOwnProperty(5), false, "shouldn't have added any elements");
assertEq(a[5], undefined);
assertEq(a.length, 4, "length shouldn't have been changed");
