var BUGNUMBER = 1184922;
var summary = "Array destructuring with various default values in various context - call/new expression";

print(BUGNUMBER + ": " + summary);

testDestructuringArrayDefault("func()");
testDestructuringArrayDefault("new func()");

if (typeof reportCompare === "function")
  reportCompare(true, true);
