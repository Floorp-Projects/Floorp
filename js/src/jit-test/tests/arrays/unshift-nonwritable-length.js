load(libdir + "asserts.js");

function f(arr)
{
  assertEq(arr.unshift(3, 5, 7, 9), 8); // if it doesn't throw :-)
}

var N = 100;

function test(out)
{
  // Create an array of arrays, to be iterated over for [].unshift-calling.  We
  // can't just loop on unshift on a single array with non-writable length
  // because unshift throws when called on an array with non-writable length.
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
  assertEq(arrs[i].length, 8, "unexpected length for arrs[" + i + "]");
  assertEq(arrs[i][0], 3, "bad element for arrs[" + i + "][0]");
  assertEq(arrs[i][1], 5, "bad element for arrs[" + i + "][1]");
  assertEq(arrs[i][2], 7, "bad element for arrs[" + i + "][2]");
  assertEq(arrs[i][3], 9, "bad element for arrs[" + i + "][3]");
  assertEq(arrs[i][4], 0, "bad element for arrs[" + i + "][4]");
  assertEq(arrs[i][5], 1, "bad element for arrs[" + i + "][5]");
  assertEq(arrs[i][6], 2, "bad element for arrs[" + i + "][6]");
  assertEq(arrs[i][7], 3, "bad element for arrs[" + i + "][7]");
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
assertEq(a.hasOwnProperty(6), false, "shouldn't have added any elements");
assertEq(a[6], undefined);
assertEq(a.hasOwnProperty(7), false, "shouldn't have added any elements");
assertEq(a[7], undefined);
assertEq(a.length, 4, "length shouldn't have been changed");
