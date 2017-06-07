function t()
{
  var a = arguments;
  Object.defineProperty(a, "length", { writable: false });
  for (var i = 0; i < 5; i++)
    assertEq(a.length, 0);

  var desc = Object.getOwnPropertyDescriptor(a, "length");
  assertEq(desc.value, 0);
  assertEq(desc.writable, false);
  assertEq(desc.enumerable, false);
  assertEq(desc.configurable, true);
}
t();
