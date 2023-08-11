// |reftest|
function assertLineAndColumn(str, line, column) {
  try {
    eval(str);
    throw 'didn\'t syntaxerror, bad.'
  } catch (e) {
    assertEq(e instanceof SyntaxError, true);
    assertEq(e.lineNumber, line);
    assertEq(e.columnNumber, column);
  }
}

assertLineAndColumn(`class A { g() { return this.#x; }}`, 1, 29);
// Make sure we're reporting the first error, if there are multiple, in class
// exit context;
assertLineAndColumn(
    `class A { g() { return this.#x; } y() { return       this.#z + this.#y; } }`,
    1, 29);
assertLineAndColumn(`this.#x`, 1, 6);
// Make sure we're reporting the first error, if there are multiple, in
// non-class context;
assertLineAndColumn(`this.#x; this.#y; this.#z`, 1, 6);

assertLineAndColumn(
    `class A {
g() { return this.#x; }}`,
    2, 19);
assertLineAndColumn(
    `class A {

g() { return this.#x; } y() { return this.#y; }}`,
    3, 19);

if (typeof reportCompare === 'function') reportCompare(0, 0);
