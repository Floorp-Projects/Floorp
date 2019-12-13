assertEq(Function.prototype.toString(), "function () {\n    [native code]\n}");
if (Function.prototype.toSource) {
    assertEq(Function.prototype.toSource(), "function () {\n    [native code]\n}");
}
