var o = {get prop() { a + b; }, set prop(x) { a + b; }};
var prop = Object.getOwnPropertyDescriptor(o, "prop");
assertEq(prop.get.toString(), "get prop() { a + b; }");
if (Function.prototype.toSource) {
    assertEq(prop.get.toSource(), "get prop() { a + b; }");
}
assertEq(prop.set.toString(), "set prop(x) { a + b; }");
if (Function.prototype.toSource) {
    assertEq(prop.set.toSource(), "set prop(x) { a + b; }");
}
if (Object.prototype.toSource) {
    assertEq(o.toSource(), "({get prop() { a + b; }, set prop(x) { a + b; }})");
}
