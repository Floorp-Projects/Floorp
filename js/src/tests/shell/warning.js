// |reftest| skip-if(!xulRuntime.shell)

var BUGNUMBER = 1170716;
var summary = 'Add js shell functions to get last warning';

print(BUGNUMBER + ": " + summary);

// Warning with JSEXN_SYNTAXERR.

enableLastWarning();
eval(`function f() { if (false) { "use asm"; } }`);

warning = getLastWarning();
assertEq(warning !== null, true);
assertEq(warning.name, "SyntaxError");
assertEq(warning.message.includes("Directive Prologue"), true);
assertEq(warning.lineNumber, 1);
assertEq(warning.columnNumber, 29);

// Disabled.

disableLastWarning();

eval(`function f() { if (false) { "use asm"; } }`);

enableLastWarning();
warning = getLastWarning();
assertEq(warning, null);

disableLastWarning();

if (typeof reportCompare === "function")
  reportCompare(true, true);
