function f(arr, i, v)
{
  arr[i] = v;
}

function test()
{
  // Use a much-greater capacity than the eventual non-writable length.
  var a = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
  Object.defineProperty(a, "length", { writable: false, value: 6 });

  for (var i = 0; i < 100; i++)
    f(a, a.length, i);

  assertEq(a.hasOwnProperty(6), false);
  assertEq(a[6], undefined);
  assertEq(a.length, 6);
}

test();
