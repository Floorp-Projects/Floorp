// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

// Valid `using` syntaxes
Reflect.parse("{ using x = {} }");
Reflect.parse("using x = {}", { target: "module" });
Reflect.parse("{ using x = fn() }");
Reflect.parse("{ using x = fn(); }");
Reflect.parse("function f() { using x = fn(); }");
Reflect.parse("switch (x) { case 1: using x = fn(); }");
Reflect.parse("if (x == 1) { using x = fn(); }");
Reflect.parse("for (let i = 0; i < 10; i++) { using x = fn(); }");
Reflect.parse("for (const i of [1, 2, 3]) { using x = fn(); }");
Reflect.parse("for (const i in {a: 1, b: 2}) { using x = fn(); }");
Reflect.parse("function* gen() { using x = fn(); }");
Reflect.parse("async function* fn() { using x = fn(); }");
Reflect.parse("async function fn() { using x = fn(); }");
Reflect.parse("class xyz { static { using x = fn(); } }");
Reflect.parse("{ using a = fn1(), b = fn2(); }");
Reflect.parse("{ using x = null }");
Reflect.parse("{ using x = undefined }");
Reflect.parse("{ for (using x of y) {} }");
Reflect.parse("for (using x of y) {}");
Reflect.parse("for await (using x of y) {}", { target: "module" });
Reflect.parse("async function fn() { for await (using x of y) {} }");

const ast = Reflect.parse("{ using x = {} }");
assertEq(ast.body[0].body[0].type, "VariableDeclaration");
assertEq(ast.body[0].body[0].kind, "using");
assertEq(ast.body[0].body[0].declarations[0].type, "VariableDeclarator");
assertEq(ast.body[0].body[0].declarations[0].id.name, "x");

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

// Valid `await using` syntaxes
Reflect.parse("await using x = {}", { target: "module" });
Reflect.parse("if (x == 1) { await using x = fn(); }", { target: "module" });
Reflect.parse("switch (x) { case 1: await using x = fn(); }", { target: "module" });
Reflect.parse("async function fn() { await using x = {}; }");
Reflect.parse("async function* gen() { await using x = {}; }");
Reflect.parse("for (let i = 0; i < 10; i++) { await using x = fn(); }", { target: "module" });
Reflect.parse("for (const i of [1, 2, 3]) { await using x = fn(); }", { target: "module" });
Reflect.parse("for (const i in {a: 1, b: 2}) { await using x = fn(); }", { target: "module" });
Reflect.parse("for (await using x of y) {}", { target: "module" });
Reflect.parse("for await (await using x of y) {}", { target: "module" });
Reflect.parse("async function fn() { for (await using x of y) {} }");
Reflect.parse("async function fn() { for await (await using x of y) {} }");

const ast16 = Reflect.parse("await using x = {}", { target: "module" });
assertEq(ast16.body[0].type, "VariableDeclaration");
assertEq(ast16.body[0].kind, "await using");
assertEq(ast16.body[0].declarations[0].type, "VariableDeclarator");
assertEq(ast16.body[0].declarations[0].id.name, "x");

const ast17 = Reflect.parse("for (await using x of y) {}", { target: "module" });
assertEq(ast17.body[0].type, "ForOfStatement");
assertEq(ast17.body[0].left.type, "VariableDeclaration");
assertEq(ast17.body[0].left.kind, "await using");
assertEq(ast17.body[0].left.declarations[0].type, "VariableDeclarator");
assertEq(ast17.body[0].left.declarations[0].id.type, "Identifier");
assertEq(ast17.body[0].left.declarations[0].id.name, "x");

const ast18 = Reflect.parse("for await (await using x of y) {}", { target: "module" });
assertEq(ast18.body[0].type, "ForOfStatement");
assertEq(ast18.body[0].left.type, "VariableDeclaration");
assertEq(ast18.body[0].left.kind, "await using");
assertEq(ast18.body[0].left.declarations[0].type, "VariableDeclarator");
assertEq(ast18.body[0].left.declarations[0].id.type, "Identifier");
assertEq(ast18.body[0].left.declarations[0].id.name, "x");

// Invalid `using` syntaxes
assertThrowsInstanceOf(() => Reflect.parse("using x = {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("{ const using x = fn() }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("{ let using x = fn() }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("{ var using x = fn() }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("{ using const x = fn() }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("{ using let x = fn() }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("{ using var x = fn() }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("{ using x = fn(), x = fn() }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (using x in y) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (using\nx of y) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (using of of y) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("{ using /a/; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (using /a/g of y) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("if(1) using x = {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (;;) using x = 10;"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("foo: using x = 10;"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("export using x = 10", { target: "module" }), SyntaxError);

// Invalid `await using` syntaxes
assertThrowsInstanceOf(() => Reflect.parse("{ await using }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using x = fn(), x = fn()", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function fn() { await using x = fn(), x = fn(); }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (await using x in y) {}", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function fn() { for (await using x in y) {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (await using\nx of y) {}", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (await\nusing x of y) {}", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function fn() { for (await\nusing x of y) {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function fn() { for (await using\nx of y) {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (await using;;) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using /a/;", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (await using /a/g of y) {}", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("if(1) await using x = {}", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (;;) await using x = 10;", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("foo: await using x = 10;", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("export await using x = 10", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (await using [x,y] of z) {}", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (await using {x,y} of z) {}", { target: "module" }), SyntaxError);

// Valid usage of `using` syntax with contextual keywords
Reflect.parse("{ using await = {}; }");
Reflect.parse("{ using yield = {}; }");
Reflect.parse("{ using public = {}; }");
Reflect.parse("{ using private = {}; }");
Reflect.parse("{ using protected = {}; }");
Reflect.parse("{ using static = {}; }");
Reflect.parse("{ using as = {}; }");
Reflect.parse("{ using async = {}; }");
Reflect.parse("{ using implements = {}; }");
Reflect.parse("{ using interface = {}; }");
Reflect.parse("{ using package = {}; }");
Reflect.parse("'use strict'; { using await = {}; }");
Reflect.parse("for (using await of y) {}");
Reflect.parse("for (using yield of y) {}");
Reflect.parse("using async = {};", { target: "module" });
Reflect.parse("await using async = {};", { target: "module" });
Reflect.parse("async function fn() { using async = {}; }");

// Valid usage of `await using` syntax with contextual keywords.
Reflect.parse("async function fn() { await using yield = {} }");
Reflect.parse("async function fn() { await using public = {} }");
Reflect.parse("async function fn() { await using private = {} }");
Reflect.parse("async function fn() { await using protected = {} }");
Reflect.parse("async function fn() { await using static = {} }");
Reflect.parse("async function fn() { await using as = {} }");
Reflect.parse("async function fn() { await using async = {} }");
Reflect.parse("async function fn() { await using implements = {} }");
Reflect.parse("async function fn() { await using package = {}; }");
Reflect.parse("await using as = {}", { target: "module" });
Reflect.parse("await using async = {};", { target: "module" });
Reflect.parse("for (await using of of y) {}", { target: "module" })

// Invalid usage of `using` syntax with contextual keywords.
assertThrowsInstanceOf(() => Reflect.parse("{ using let = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("using await = {}", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function fn() { using await = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function fn() { for (using await of y) {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; { using yield = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; { using public = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; { using private = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; { using protected = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; { using static = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; { using implements = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; { using interface = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; { using package = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; for (using yield of y) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("function* gen() { using yield = {}; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("for (using await of y) {}", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function fn() { for (using await of y) {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("function* gen() { for (using yield of y) {} }"), SyntaxError);

// Invalid usage of `await using` syntax with contextual keywords.
assertThrowsInstanceOf(() => Reflect.parse("await using yield = {};", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using public = {};", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using private = {};", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using protected = {};", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using static = {};", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using implements = {};", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using interface = {};", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using package = {};", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("await using await = {};", { target: "module" }), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function fn() { await using await = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async function fn() { await using await = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async function fn() { await using yield = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async function fn() { await using public = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async function fn() { await using private = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async function fn() { await using protected = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async function fn() { await using static = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async function fn() { await using implements = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async function fn() { await using package = {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function* gen() { await using yield = {}; } "), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function fn() { for (await using await of y) {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("async function* gen() { for (await using yield of y) {} }"), SyntaxError);

// Valid syntaxes close to `using` but not `using` declarations.
Reflect.parse("for (using of y) {}");
Reflect.parse("for (using of of) {}");
Reflect.parse("for (using\nof y) {}");
Reflect.parse("{ const using = 10; }");
Reflect.parse("{ let using = 10 }");
Reflect.parse("{ var using = 10 }");
Reflect.parse("{ using = 10 }");
Reflect.parse("{ using + 1 }");
Reflect.parse("{ using++ }");
Reflect.parse("{ using\nx = 10 }");
Reflect.parse("{ using = {x: 10} }");
Reflect.parse("{ x = { using: 10 } }");
Reflect.parse("{ x.using = 10 }");
Reflect.parse("{ x\n.using = 10 }");
Reflect.parse("{ using.x = 10 }");
Reflect.parse("{ using\n.x = 10 }");
Reflect.parse("for (using[1] of {}) {}");
Reflect.parse("for (using\n[1] of {}) {}")
Reflect.parse("for (using.x of {}) {}");
Reflect.parse("for (using\n.x of {}) {}");
Reflect.parse("for (x.using in {}) {}");
Reflect.parse("for (x\n.using in {}) {}")
Reflect.parse("{ using: x = 10; }");
Reflect.parse("{ using\n: x = 10; }");
Reflect.parse("{ using /a/g; }");
Reflect.parse("{ /using/g }");
Reflect.parse("{ using\nlet = {} }");
Reflect.parse("export const using = 10", { target: "module" });
Reflect.parse("import using from 'xyz'", { target: "module" });

const ast4 = Reflect.parse("{ using = 10 }");
assertEq(ast4.body[0].body[0].type, "ExpressionStatement");
assertEq(ast4.body[0].body[0].expression.type, "AssignmentExpression");
assertEq(ast4.body[0].body[0].expression.left.type, "Identifier");
assertEq(ast4.body[0].body[0].expression.left.name, "using");

const ast5 = Reflect.parse("for (using of y) {}");
assertEq(ast5.body[0].type, "ForOfStatement");
assertEq(ast5.body[0].left.type, "Identifier");
assertEq(ast5.body[0].left.name, "using");

const ast6 = Reflect.parse("{ using + 1 }");
assertEq(ast6.body[0].body[0].type, "ExpressionStatement");
assertEq(ast6.body[0].body[0].expression.type, "BinaryExpression");
assertEq(ast6.body[0].body[0].expression.left.type, "Identifier");
assertEq(ast6.body[0].body[0].expression.left.name, "using");

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

const ast11 = Reflect.parse("{ /using/g }");
assertEq(ast11.body[0].body[0].type, "ExpressionStatement");
assertEq(ast11.body[0].body[0].expression.type, "Literal");
assertEq(ast11.body[0].body[0].expression.value.source, "using");
assertEq(ast11.body[0].body[0].expression.value.flags, "g");

const ast12 = Reflect.parse("{ using: x = 10; }");
assertEq(ast12.body[0].body[0].type, "LabeledStatement");
assertEq(ast12.body[0].body[0].label.type, "Identifier");
assertEq(ast12.body[0].body[0].label.name, "using");

const ast13 = Reflect.parse("{ using\n: x = 10; }");
assertEq(ast13.body[0].body[0].type, "LabeledStatement");
assertEq(ast13.body[0].body[0].label.type, "Identifier");
assertEq(ast13.body[0].body[0].label.name, "using");

const ast14 = Reflect.parse("{ using /a/g; }");
// should be parsed as division, not regex
assertEq(ast14.body[0].body[0].type, "ExpressionStatement");
assertEq(ast14.body[0].body[0].expression.type, "BinaryExpression");
assertEq(ast14.body[0].body[0].expression.operator, "/");
assertEq(ast14.body[0].body[0].expression.left.type, "BinaryExpression");
assertEq(ast14.body[0].body[0].expression.left.operator, "/");

const ast15 = Reflect.parse("import using from 'xyz'", { target: "module" });
assertEq(ast15.body[0].type, "ImportDeclaration");
assertEq(ast15.body[0].specifiers[0].type, "ImportSpecifier");
assertEq(ast15.body[0].specifiers[0].name.name, "using");

// Valid syntaxes close to `await using` but not `await using` declarations.
Reflect.parse("await /xyz/g");
Reflect.parse("{ await /using/g }");
Reflect.parse("async function fn() { await using; }");
Reflect.parse("async function fn() { await\nusing; }")
Reflect.parse("async function fn() { await /using/g }");
Reflect.parse("async function fn() { await using/g }");
Reflect.parse("async function fn() { await using/\ng }");
Reflect.parse("async function fn() { for(await using;;) {} }");
Reflect.parse("await using;", { target: "module" });
Reflect.parse("await\nusing;", { target: "module" });
Reflect.parse("await /using/g", { target: "module" });
Reflect.parse("await using/g", { target: "module" });
Reflect.parse("await using/\ng", { target: "module" });
Reflect.parse("for(await using;;) {}", { target: "module" });
Reflect.parse("await using\nx;", { target: "module" });

const ast19 = Reflect.parse("await using", { target: "module" });
assertEq(ast19.body[0].type, "ExpressionStatement");
assertEq(ast19.body[0].expression.type, "UnaryExpression");
assertEq(ast19.body[0].expression.operator, "await");
assertEq(ast19.body[0].expression.argument.type, "Identifier");
assertEq(ast19.body[0].expression.argument.name, "using");

const ast20 = Reflect.parse("await /using/g", { target: "module" });
assertEq(ast20.body[0].type, "ExpressionStatement");
assertEq(ast20.body[0].expression.type, "UnaryExpression");
assertEq(ast20.body[0].expression.operator, "await");
assertEq(ast20.body[0].expression.argument.type, "Literal");
assertEq(ast20.body[0].expression.argument.value.source, "using");
assertEq(ast20.body[0].expression.argument.value.flags, "g");

const ast21 = Reflect.parse("await using/g", { target: "module" });
assertEq(ast21.body[0].type, "ExpressionStatement");
assertEq(ast21.body[0].expression.type, "BinaryExpression");
assertEq(ast21.body[0].expression.operator, "/");
assertEq(ast21.body[0].expression.left.type, "UnaryExpression");
assertEq(ast21.body[0].expression.left.operator, "await");
assertEq(ast21.body[0].expression.left.argument.type, "Identifier");
assertEq(ast21.body[0].expression.left.argument.name, "using");

const ast22 = Reflect.parse("for(await using;;) {}", { target: "module" });
assertEq(ast22.body[0].type, "ForStatement");
assertEq(ast22.body[0].init.type, "UnaryExpression");
assertEq(ast22.body[0].init.operator, "await");
assertEq(ast22.body[0].init.argument.type, "Identifier");
assertEq(ast22.body[0].init.argument.name, "using");
