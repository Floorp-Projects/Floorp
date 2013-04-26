function f(arr)
{
  assertEq(arr.push(4), 5); // if it doesn't throw :-)
}

function test(out)
{
  // Create an array of arrays, to be iterated over for [].push-calling.  We
  // can't just loop on push on a single array with non-writable length because
  // push throws when called on an array with non-writable length.
  var arrs = out.arrs = [];
  for (var i = 0; i < 100; i++)
    arrs.push([0, 1, 2, 3]);

  // Use a much-greater capacity than the eventual non-writable length, so that
  // the inline-push will work.
  var a = [0, 1, 2, 3, 4, 5, 6, 7];
  Object.defineProperty(a, "length", { writable: false, value: 4 });

  arrs.push(a);

  for (var i = 0, sz = arrs.length; i < sz; i++)
  {
    var arr = arrs[i];
    f(arr);
  }
}

var obj = {};
var a, arrs;

try
{
  test(obj);
  throw new Error("didn't throw!");
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "expected TypeError, got " + e);

  arrs = obj.arrs;
  assertEq(arrs.length, 101);
  for (var i = 0; i < 100; i++)
  {
    assertEq(arrs[i].length, 5, "unexpected length for arrs[" + i + "]");
    assertEq(arrs[i][0], 0, "bad element for arrs[" + i + "][0]");
    assertEq(arrs[i][1], 1, "bad element for arrs[" + i + "][1]");
    assertEq(arrs[i][2], 2, "bad element for arrs[" + i + "][2]");
    assertEq(arrs[i][3], 3, "bad element for arrs[" + i + "][3]");
    assertEq(arrs[i][4], 4, "bad element for arrs[" + i + "][4]");
  }

  a = arrs[100];
  assertEq(a[0], 0, "bad element for a[" + i + "]");
  assertEq(a[1], 1, "bad element for a[" + i + "]");
  assertEq(a[2], 2, "bad element for a[" + i + "]");
  assertEq(a[3], 3, "bad element for a[" + i + "]");
  assertEq(a.hasOwnProperty(4), false, "element addition should have thrown");
  assertEq(a[4], undefined);
  assertEq(a.length, 4, "length shouldn't have been changed");
}
