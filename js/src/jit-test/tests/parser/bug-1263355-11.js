function assertNotSame(expected, actual, message = "") { }
function g3(h = () => arguments) {
  function arguments() { }
  assertNotSame(arguments, h());
}
g3();
