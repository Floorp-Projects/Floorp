function t()
{
  Object.defineProperty(arguments, "length", { value: 17 });
  for (var i = 0; i < 5; i++)
    assertEq(arguments.length, 17);
}
t();
