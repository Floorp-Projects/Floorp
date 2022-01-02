function t()
{
  var a = arguments;
  Object.defineProperty(a, "length", { enumerable: true });
  for (var i = 0; i < 5; i++)
    assertEq(a.length, 0);

  var desc = Object.getOwnPropertyDescriptor(a, "length");
  assertEq(desc.value, 0);
  assertEq(desc.writable, true);
  assertEq(desc.enumerable, true);
  assertEq(desc.configurable, true);
}
t();
