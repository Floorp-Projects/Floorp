// |jit-test| skip-if: !('oomTest' in this)

load(libdir + "asserts.js");

assertEq(checkRegExpSyntax("correct[reg]exp"), undefined);
let err = checkRegExpSyntax("regex[withSyntaxError");
assertEq(err instanceof SyntaxError, true);

oomTest(() => checkRegExpSyntax("correct(re)gexp"))

var checkReturnedSyntaxError = true;
oomTest(() => {
    let err = checkRegExpSyntax("regex[withSyntaxError");
    if (!(err instanceof SyntaxError)) { checkReturnedSyntaxError = false; }
})
assertEq(checkReturnedSyntaxError, true);
