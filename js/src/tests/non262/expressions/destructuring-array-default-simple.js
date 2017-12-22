var BUGNUMBER = 1184922;
var summary = "Array destructuring with various default values in various context - simple literal";

print(BUGNUMBER + ": " + summary);

testDestructuringArrayDefault("'foo'");
testDestructuringArrayDefault("`foo`");
testDestructuringArrayDefault("func`foo`");

testDestructuringArrayDefault("/foo/");

testDestructuringArrayDefault("{}");
testDestructuringArrayDefault("[]");

if (typeof reportCompare === "function")
  reportCompare(true, true);
