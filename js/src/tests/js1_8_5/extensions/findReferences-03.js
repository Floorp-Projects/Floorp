// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Jim Blandy

if (typeof findReferences == "function") {

    function makeGenerator(c) { eval(c); yield function generatorClosure() { return x; }; }
    var generator = makeGenerator('var x = 42');
    var closure = generator.next();
    referencesVia(closure, 'parent; generator object', generator);

    var o = {};

    assertEq(function f() {
	return referencesVia(null, 'arguments', arguments) ||
	       referencesVia(null, 'baseline-args-obj', arguments);
    }(), true);

    var rvalueCorrect;

    function finallyHoldsRval() {
        try {
            return o;
        } finally {
            rvalueCorrect = referencesVia(null, 'rval', o) ||
                            referencesVia(null, 'baseline-rval', o);
        }
    }
    rvalueCorrect = false;
    finallyHoldsRval();
    assertEq(rvalueCorrect, true);

    // Because we don't distinguish between JavaScript stack marking and C++
    // stack marking (both use the conservative scanner), we can't really write
    // the following tests meaningfully:
    //   generator frame -> generator object
    //   stack frame -> local variables
    //   stack frame -> this
    //   stack frame -> callee
    //   for(... in x) loop's reference to x

    reportCompare(true, true);
} else {
    reportCompare(true, true, "test skipped: findReferences is not a function");
}
