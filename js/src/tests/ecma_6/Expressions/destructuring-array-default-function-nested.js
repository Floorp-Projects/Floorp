var BUGNUMBER = 1184922;
var summary = "Array destructuring with various default values in various context - function expression with nested objects";

print(BUGNUMBER + ": " + summary);

testDestructuringArrayDefault("function f() { return { f() {}, *g() {}, r: /a/ }; }");
testDestructuringArrayDefault("function* g() { return { f() {}, *g() {}, r: /b/ }; }");
testDestructuringArrayDefault("() => { return { f() {}, *g() {}, r: /c/ }; }");

if (typeof reportCompare === "function")
  reportCompare(true, true);
