// export-from should throw SyntaxError until it's implemented.

var caught = false;
try {
  eval("export { a } from 'b';");
} catch (e) {
  caught = true;
}
assertEq(caught, true);
