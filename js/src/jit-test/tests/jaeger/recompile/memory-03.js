
eval("var x = 10; function foo() { return x; }");

assertEq(foo(), 10);
gc();
assertEq(foo(), 10);
