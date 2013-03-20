load(libdir + "asserts.js");

function f(arr, v1, v2)
{
  // Ensure array_push_slowly is called by passing more than one argument.
  arr.push(v1, v2);
}

var N = 100;

function test(out)
{
  // Create an array of arrays, to be iterated over for [].push-calling.  We
  // can't just loop on push on a single array with non-writable length because
  // push throws when called on an array with non-writable length.
  var arrs = out.arrs = [];
  for (var i = 0; i < N; i++)
    arrs.push([]);

  // Use a much-greater capacity than the eventual non-writable length.
  var a = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
  Object.defineProperty(a, "length", { writable: false, value: 6 });

  arrs.push(a);

  for (var i = 0, sz = arrs.length; i < sz; i++)
  {
    var arr = arrs[i];
    f(arr, 8675309, 3141592);
  }
}

var obj = {};

assertThrowsInstanceOf(function() { test(obj); }, TypeError);

var arrs = obj.arrs;
assertEq(arrs.length, N + 1);
for (var i = 0; i < N; i++)
{
  assertEq(arrs[i].length, 2, "unexpected length for arrs[" + i + "]");
  assertEq(arrs[i][0], 8675309, "bad element for arrs[" + i + "][0]");
  assertEq(arrs[i][1], 3141592, "bad element for arrs[" + i + "][1]");
}

var a = arrs[N];
assertEq(a.hasOwnProperty(6), false);
assertEq(a[6], undefined);
assertEq(a.hasOwnProperty(7), false);
assertEq(a[7], undefined);
assertEq(a.length, 6);
