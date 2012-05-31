// Compiling a script with null filename does not break the Error constructor.

var exc = null;
try {
    evaluate("throw Error('pass');", {fileName: null});
} catch (x) {
    exc = x;
}
assertEq(exc.constructor, Error);
assertEq(exc.message, "pass");
assertEq(exc.fileName, "");
