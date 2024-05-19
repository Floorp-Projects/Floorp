// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

Reflect.parse("using x = {}");
Reflect.parse("using x = fn()");
Reflect.parse("{ using x = fn(); }");
Reflect.parse("function f() { using x = fn(); }");
Reflect.parse("using a = fn1(), b = fn2();");
Reflect.parse("for (using x of y) {}");
Reflect.parse("async function fn() { for await (using x of y) {} }");
Reflect.parse("using x = null");
Reflect.parse("using x = undefined");
// Existing syntaxes that should not break
Reflect.parse("for (using of y) {}");
Reflect.parse("for (using of of) {}");
Reflect.parse("for (using\nof y) {}");
Reflect.parse("const using = 10");
Reflect.parse("let using = 10");
Reflect.parse("var using = 10");
Reflect.parse("using = 10");
Reflect.parse("using + 1");
Reflect.parse("using++");
Reflect.parse("using\nx = 10");
Reflect.parse("using = {x: 10}");
Reflect.parse("x = { using: 10 }");
Reflect.parse("x.using = 10");
Reflect.parse("x\n.using = 10");
Reflect.parse("using.x = 10");
Reflect.parse("using\n.x = 10");
Reflect.parse("for (using[1] of {}) {}");
Reflect.parse("for (using\n[1] of {}) {}")
Reflect.parse("for (using.x of {}) {}");
Reflect.parse("for (using\n.x of {}) {}");
Reflect.parse("for (x.using in {}) {}");
Reflect.parse("for (x\n.using in {}) {}")
Reflect.parse("using: x = 10;");
Reflect.parse("using\n: x = 10;");
Reflect.parse("using /a/g;");
Reflect.parse("/using/g");
Reflect.parse("export const using = 10", { target: "module" });
Reflect.parse("import using from 'xyz'", { target: "module" });

// ast checks
const ast = Reflect.parse("using x = {}");
assertEq(ast.body[0].type, "VariableDeclaration");
assertEq(ast.body[0].kind, "using");
assertEq(ast.body[0].declarations[0].type, "VariableDeclarator");
assertEq(ast.body[0].declarations[0].id.name, "x");

const ast2 = Reflect.parse("for (using x of y) {}");
assertEq(ast2.body[0].type, "ForOfStatement");
assertEq(ast2.body[0].left.type, "VariableDeclaration");
assertEq(ast2.body[0].left.kind, "using");
assertEq(ast2.body[0].left.declarations[0].type, "VariableDeclarator");
assertEq(ast2.body[0].left.declarations[0].id.type, "Identifier");
assertEq(ast2.body[0].left.declarations[0].id.name, "x");

const ast3 = Reflect.parse("async function fn() { for await (using x of y) {} }");
const forStatement = ast3.body[0].body.body[0];
assertEq(forStatement.type, "ForOfStatement");
assertEq(forStatement.left.type, "VariableDeclaration");
assertEq(forStatement.left.kind, "using");
assertEq(forStatement.left.declarations[0].type, "VariableDeclarator");
assertEq(forStatement.left.declarations[0].id.type, "Identifier");
assertEq(forStatement.left.declarations[0].id.name, "x");

const ast4 = Reflect.parse("using = 10");
assertEq(ast4.body[0].type, "ExpressionStatement");
assertEq(ast4.body[0].expression.type, "AssignmentExpression");
assertEq(ast4.body[0].expression.left.type, "Identifier");
assertEq(ast4.body[0].expression.left.name, "using");

const ast5 = Reflect.parse("for (using of y) {}");
assertEq(ast5.body[0].type, "ForOfStatement");
assertEq(ast5.body[0].left.type, "Identifier");
assertEq(ast5.body[0].left.name, "using");

const ast6 = Reflect.parse("using + 1");
assertEq(ast6.body[0].type, "ExpressionStatement");
assertEq(ast6.body[0].expression.type, "BinaryExpression");
assertEq(ast6.body[0].expression.left.type, "Identifier");
assertEq(ast6.body[0].expression.left.name, "using");

const ast7 = Reflect.parse("for (using of of) {}");
assertEq(ast7.body[0].type, "ForOfStatement");
assertEq(ast7.body[0].left.type, "Identifier");
assertEq(ast7.body[0].left.name, "using");
assertEq(ast7.body[0].right.type, "Identifier");
assertEq(ast7.body[0].right.name, "of");

const ast8 = Reflect.parse("for (using\nof y) {}");
assertEq(ast8.body[0].type, "ForOfStatement");
assertEq(ast8.body[0].left.type, "Identifier");
assertEq(ast8.body[0].left.name, "using");
assertEq(ast8.body[0].right.type, "Identifier");
assertEq(ast8.body[0].right.name, "y");

const ast9 = Reflect.parse("for (using[1] of {}) {}");
assertEq(ast9.body[0].type, "ForOfStatement");
assertEq(ast9.body[0].left.type, "MemberExpression");
assertEq(ast9.body[0].left.object.type, "Identifier");
assertEq(ast9.body[0].left.object.name, "using");
assertEq(ast9.body[0].left.property.type, "Literal");
assertEq(ast9.body[0].left.property.value, 1);

const ast10 = Reflect.parse("for (using\n[1] of {}) {}");
assertEq(ast10.body[0].type, "ForOfStatement");
assertEq(ast10.body[0].left.type, "MemberExpression");
assertEq(ast10.body[0].left.object.type, "Identifier");
assertEq(ast10.body[0].left.object.name, "using");
assertEq(ast10.body[0].left.property.type, "Literal");
assertEq(ast10.body[0].left.property.value, 1);

const ast11 = Reflect.parse("/using/g");
assertEq(ast11.body[0].type, "ExpressionStatement");
assertEq(ast11.body[0].expression.type, "Literal");
assertEq(ast11.body[0].expression.value.source, "using");
assertEq(ast11.body[0].expression.value.flags, "g");

const ast12 = Reflect.parse("using: x = 10;");
assertEq(ast12.body[0].type, "LabeledStatement");
assertEq(ast12.body[0].label.type, "Identifier");
assertEq(ast12.body[0].label.name, "using");

const ast13 = Reflect.parse("using\n: x = 10;");
assertEq(ast13.body[0].type, "LabeledStatement");
assertEq(ast13.body[0].label.type, "Identifier");
assertEq(ast13.body[0].label.name, "using");

const ast14 = Reflect.parse("using /a/g;");
// should be parsed as division, not regex
assertEq(ast14.body[0].type, "ExpressionStatement");
assertEq(ast14.body[0].expression.type, "BinaryExpression");
assertEq(ast14.body[0].expression.operator, "/");
assertEq(ast14.body[0].expression.left.type, "BinaryExpression");
assertEq(ast14.body[0].expression.left.operator, "/");

const ast15 = Reflect.parse("import using from 'xyz'", { target: "module" });
assertEq(ast15.body[0].type, "ImportDeclaration");
assertEq(ast15.body[0].specifiers[0].type, "ImportSpecifier");
assertEq(ast15.body[0].specifiers[0].name.name, "using");

assertThrowsInstanceOf(() => Reflect.parse("const using x = fn()"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("let using x = fn()"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("var using x = fn()"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("using const x = fn()"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("using let x = fn()"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("using var x = fn()"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("using x = fn(), x = fn()"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (using x in y) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (using\nx of y) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (using of of y) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("using /a/;"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (using /a/g of y) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("if(1) using x = {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (;;) using x = 10;"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("foo: using x = 10;"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("export using x = 10", { target: "module" }), SyntaxError);