var BUGNUMBER = 1184922;
var summary = "Array destructuring with various default values in various context - function expression";

print(BUGNUMBER + ": " + summary);

testDestructuringArrayDefault("function f() {}");
testDestructuringArrayDefault("function* g() {}");
testDestructuringArrayDefault("() => {}");

if (typeof reportCompare === "function")
  reportCompare(true, true);
