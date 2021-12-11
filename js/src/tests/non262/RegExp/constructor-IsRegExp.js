var BUGNUMBER = 1147817;
var summary = "RegExp constructor with pattern with @@match.";

print(BUGNUMBER + ": " + summary);

var matchValue;
var constructorValue;

var matchGet;
var constructorGet;
var sourceGet;
var flagsGet;
function reset() {
  matchGet = false;
  constructorGet = false;
  sourceGet = false;
  flagsGet = false;
}
var obj = {
  get [Symbol.match]() {
    matchGet = true;
    return matchValue;
  },
  get constructor() {
    constructorGet = true;
    return constructorValue;
  },
  get source() {
    sourceGet = true;
    return "foo";
  },
  get flags() {
    flagsGet = true;
    return "i";
  },
  toString() {
    return "bar";
  }
};

matchValue = true;
constructorValue = function() {};

reset();
assertEq(RegExp(obj).toString(), "/foo/i");
assertEq(matchGet, true);
assertEq(constructorGet, true);
assertEq(sourceGet, true);
assertEq(flagsGet, true);

reset();
assertEq(RegExp(obj, "g").toString(), "/foo/g");
assertEq(matchGet, true);
assertEq(constructorGet, false);
assertEq(sourceGet, true);
assertEq(flagsGet, false);

matchValue = false;
constructorValue = function() {};

reset();
assertEq(RegExp(obj).toString(), "/bar/");
assertEq(matchGet, true);
assertEq(constructorGet, false);
assertEq(sourceGet, false);
assertEq(flagsGet, false);

reset();
assertEq(RegExp(obj, "g").toString(), "/bar/g");
assertEq(matchGet, true);
assertEq(constructorGet, false);
assertEq(sourceGet, false);
assertEq(flagsGet, false);

matchValue = true;
constructorValue = RegExp;

reset();
assertEq(RegExp(obj), obj);
assertEq(matchGet, true);
assertEq(constructorGet, true);
assertEq(sourceGet, false);
assertEq(flagsGet, false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
