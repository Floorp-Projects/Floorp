var f = Function("a", "b", "return a + b;");
assertEq(f.toString(), "function anonymous(a, b) { return a + b; }");
assertEq(f.toSource(), "(function anonymous(a, b) { return a + b; })");
assertEq(decompileFunction(f), f.toString());
assertEq(decompileBody(f), "return a + b;");
f = Function("a", "...rest", "return rest[42] + b;");
assertEq(f.toString(), "function anonymous(a, ...rest) { return rest[42] + b; }");
assertEq(f.toSource(), "(function anonymous(a, ...rest) { return rest[42] + b; })")
assertEq(decompileFunction(f), f.toString());
assertEq(decompileBody(f), "return rest[42] + b;");
f = Function("x", "return let (y) x;");
assertEq(f.toSource(), "(function anonymous(x) { return let (y) x; })");
f = Function("");
assertEq(f.toString(), "function anonymous() {  }");
f = Function("", "(abc)");
assertEq(f.toString(), "function anonymous() { (abc) }");
f = Function("", "return function (a, b) a + b;")();
assertEq(f.toString(), "function (a, b) a + b");
