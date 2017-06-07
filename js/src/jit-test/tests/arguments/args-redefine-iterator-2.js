function t()
{
  var a = arguments;
  Object.defineProperty(a, Symbol.iterator, { writable: false, enumerable: true, configurable: false });
  for (var i = 0; i < 5; i++)
    assertEq(a[Symbol.iterator], Array.prototype[Symbol.iterator]);

  var desc = Object.getOwnPropertyDescriptor(a, Symbol.iterator);
  assertEq(desc.value, Array.prototype[Symbol.iterator]);
  assertEq(desc.writable, false);
  assertEq(desc.enumerable, true);
  assertEq(desc.configurable, false);
}
t();
