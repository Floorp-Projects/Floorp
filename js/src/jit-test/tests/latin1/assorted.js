// Reflect.parse Latin1
var ast = Reflect.parse(toLatin1("function f() { return 3; }"));
assertEq(ast.body[0].id.name, "f");

// Reflect.parse TwoByte
var ast = Reflect.parse("function f\u1200() { return 3; }");
assertEq(ast.body[0].id.name, "f\u1200");
