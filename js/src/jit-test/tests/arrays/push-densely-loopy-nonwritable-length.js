// Force recognition of a known-constant.
var push = Array.prototype.push;

function f(arr)
{
  // Push an actual constant to trigger JIT-inlining of the effect of the push.
  push.call(arr, 99);
}

function basic(out)
{
  // Create an array of arrays, to be iterated over for [].push-calling.  We
  // can't just loop on push on a single array with non-writable length because
  // push throws when called on an array with non-writable length.
  var arrs = out.arrs = [];
  for (var i = 0; i < 100; i++)
    arrs.push([]);

  // Use a much-greater capacity than the eventual non-writable length.
  var a = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
  Object.defineProperty(a, "length", { writable: false, value: 6 });

  arrs.push(a);

  for (var i = 0, sz = arrs.length; i < sz; i++)
  {
    var arr = arrs[i];
    f(arr);
  }
}

var obj = {};
var arrs, a;

try
{
  basic(obj);
  throw new Error("didn't throw!");
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "expected TypeError, got " + e);

  arrs = obj.arrs;
  assertEq(arrs.length, 101);
  for (var i = 0; i < 100; i++)
  {
    assertEq(arrs[i].length, 1, "unexpected length for arrs[" + i + "]");
    assertEq(arrs[i][0], 99, "bad element for arrs[" + i + "]");
  }

  a = arrs[100];
  assertEq(a.hasOwnProperty(6), false);
  assertEq(a[6], undefined);
  assertEq(a.length, 6);
}
