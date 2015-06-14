// |reftest| skip-if(!xulRuntime.shell)

var BUGNUMBER = 1170716;
var summary = 'Add js shell functions to get last warning';

print(BUGNUMBER + ": " + summary);

// Warning with JSEXN_NONE.

enableLastWarning();

eval(`({}).__proto__ = {};`);

var warning = getLastWarning();
assertEq(warning !== null, true);
assertEq(warning.name, "None");
assertEq(warning.message.includes("mutating"), true);
assertEq(warning.lineNumber, 1);
assertEq(warning.columnNumber, 2);

// Clear last warning.

clearLastWarning();
warning = getLastWarning();
assertEq(warning, null);

// Warning with JSEXN_SYNTAXERR.

options("strict");
eval(`var a; if (a=0) {}`);

warning = getLastWarning();
assertEq(warning !== null, true);
assertEq(warning.name, "SyntaxError");
assertEq(warning.message.includes("equality"), true);
assertEq(warning.lineNumber, 1);
assertEq(warning.columnNumber, 14);

// Disabled.

disableLastWarning();

eval(`var a; if (a=0) {}`);

enableLastWarning();
warning = getLastWarning();
assertEq(warning, null);

disableLastWarning();

if (typeof reportCompare === "function")
  reportCompare(true, true);
