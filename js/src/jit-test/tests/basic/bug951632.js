enableShellAllocationMetadataBuilder();
var g = newGlobal()
g.eval("function f(a) { return h(); }");
g.h = function () {
    return [1, 2, 3];
};
var o = getAllocationMetadata(g.f(5));
assertEq(o.stack.length, 1);
assertEq(o.stack[0], g.h);
