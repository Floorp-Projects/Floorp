// Debugger.Object.prototype.{errorLineNumber,errorColumnNumber} return the
// line number and column number associated with some error object.

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

var syntaxError = gw.executeInGlobal("\nlet a, a;").throw;
assertEq(syntaxError.errorLineNumber, 2);
assertEq(syntaxError.errorColumnNumber, 7);

var typeError = gw.executeInGlobal("\n1 + f();").throw;
assertEq(typeError.errorLineNumber, 2);
assertEq(typeError.errorColumnNumber, 1);

// Custom errors have no line/column numbers .
var customError = gw.executeInGlobal("\nthrow 1;").throw;
assertEq(customError.errorLineNumber, undefined);
assertEq(customError.errorColumnNumber, undefined);

customError = gw.executeInGlobal("\nthrow { errorLineNumber: 10, errorColumnNumber: 20 };").throw;
assertEq(customError.errorLineNumber, undefined);
assertEq(customError.errorColumnNumber, undefined);

customError = gw.executeInGlobal("\nthrow { lineNumber: 10, columnNumber: 20 };").throw;
assertEq(customError.errorLineNumber, undefined);
assertEq(customError.errorColumnNumber, undefined);

// Ensure that the method works across globals.
g.eval(`var g = newGlobal();
        g.eval('var err; \\n' +
               'try {\\n' +
               '  f();\\n' +
               '} catch (e) {\\n' +
               '  err = e;\\n' +
               '}');
        var err2 = g.err;`);
var otherGlobalError = gw.executeInGlobal("throw err2").throw;
assertEq(otherGlobalError.errorLineNumber, 3);
assertEq(otherGlobalError.errorColumnNumber, 3);

// Ensure that non-error objects return undefined.
const Args = [
    "1",
    "'blah'",
    "({})",
    "[]",
    "() => 1"
]

for (let arg of Args) {
    let nonError = gw.executeInGlobal(`${arg}`).return;
    assertEq(nonError.errorLineNumber, undefined);
    assertEq(nonError.errorColumnNumber, undefined);
}
