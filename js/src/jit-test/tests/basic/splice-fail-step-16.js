/* Test that arrays resize normally during splice, even if .length is non-writable. */

var arr = [1, 2, 3, 4, 5, 6];

Object.defineProperty(arr, "length", {writable: false});

try
{
  var removed = arr.splice(3, 3, 9, 9, 9, 9);
  throw new Error("splice didn't throw, returned [" + removed + "]");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "should have thrown a TypeError, instead threw " + e + ", arr is " + arr);
}

// The exception should happen in step 16, which means we've already removed the array elements.
assertEq(arr[0], 1);
assertEq(arr[1], 2);
assertEq(arr[2], 3);
assertEq(arr[3], 9);
assertEq(arr[4], 9);
assertEq(arr[5], 9);
assertEq(arr.length, 6);
