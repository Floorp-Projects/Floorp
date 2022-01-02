load(libdir + "asserts.js");

function f(arr, v1, v2)
{
  // Ensure array_push_slowly is called by passing more than one argument.
  arr.push(v1, v2);
}

function basic()
{
  // Use a much-greater capacity than the eventual non-writable length.
  var a = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
  Object.defineProperty(a, "length", { writable: false, value: 6 });

  assertThrowsInstanceOf(() => f(a, 8675309, 3141592), TypeError);

  assertEq(a.hasOwnProperty(6), false);
  assertEq(a[6], undefined);
  assertEq(a.hasOwnProperty(7), false);
  assertEq(a[7], undefined);
  assertEq(a.length, 6);
}

basic();
