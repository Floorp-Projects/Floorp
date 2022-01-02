// |jit-test| skip-if: !Function.prototype.toSource

// Source string has balanced parentheses even when the source code was discarded.

function test() {
eval("var f = function() { return 0; };");
assertEq(f.toSource(), "(function() {\n    [native code]\n})");
}

var g = newGlobal({ discardSource: true });
g.evaluate(test.toString() + "test()");
