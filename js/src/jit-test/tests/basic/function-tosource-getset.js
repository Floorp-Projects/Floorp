var o = {get prop() a + b, set prop(x) a + b};
var prop = Object.getOwnPropertyDescriptor(o, "prop");
assertEq(prop.get.toString(), "function get prop() a + b");
assertEq(prop.get.toSource(), "(function get prop() a + b)");
assertEq(prop.set.toString(), "function set prop(x) a + b");
assertEq(prop.set.toSource(), "(function set prop(x) a + b)");
assertEq(o.toSource(), "({get prop () a + b, set prop (x) a + b})");
