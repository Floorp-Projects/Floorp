var x = {get prop() { return 2; }, set prop(v) {}};
var y = {__proto__: x, prop: 1};

assertEq(y.__lookupGetter__("prop"), undefined);
assertEq(y.__lookupSetter__("prop"), undefined);
