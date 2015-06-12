// |reftest| skip-if(!xulRuntime.shell)

var BUGNUMBER = 1142351;
var summary = 'Add console warnings for non-standard flag argument of String.prototype.{search,match,replace}.';

printBugNumber(BUGNUMBER);
printStatus (summary);

function assertWarningForComponent(code, name) {
  enableLastWarning();
  var g = newGlobal();
  g.eval(code);
  var warning = getLastWarning();
  assertEq(warning !== null, true);
  assertEq(warning.name, name);
  disableLastWarning();
}

assertWarningForComponent(`'aaaA'.match('a', 'i');`, "None");
assertWarningForComponent(`'aaaA'.search('a', 'i');`, "None");
assertWarningForComponent(`'aaaA'.replace('a', 'b', 'g');`, "None");

if (typeof reportCompare === "function")
  reportCompare(true, true);
