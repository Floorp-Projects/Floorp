function t()
{
  var a = arguments;
  Object.defineProperty(a, "length", { value: 17 });
  for (var i = 0; i < 5; i++)
    assertEq(a.length, 17);
}
t();
