load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
    Reflect.parse("module foo { }");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    Reflect.parse("function foo() { module 'bar' {} }");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    Reflect.parse("if (true) { module 'foo' {} }");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    Reflect.parse("module 'foo' { if (true) { module 'bar' {} } }");
}, SyntaxError);

var node = Reflect.parse("module 'foo' {}");
assertEq(typeof node == "object" && node != null, true);
assertEq(node.type, "Program");
node = node.body;
assertEq(typeof node == "object" && node != null, true);
assertEq(node.length, 1);
node = node[0];
assertEq(typeof node == "object" && node != null, true);
assertEq(node.type, "ModuleDeclaration");
assertEq(node.name, "foo");
node = node.body;
assertEq(typeof node == "object" && node != null, true);
assertEq(node.type, "BlockStatement");
node = node.body;
assertEq(typeof node == "object" && node != null, true);
assertEq(node.length, 0);
