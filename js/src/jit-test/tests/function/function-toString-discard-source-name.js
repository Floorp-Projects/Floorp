// Repeats the test from 'function/function-toString-discard-source.js' and
// additionally verifies the name matches the expected value.
//
// This behaviour is not required by the ECMAScript standard.

// The Function.prototype.toString() representation of sourceless functions
// must match the NativeFunction syntax.

function test() {
// Greatly (!) simplified patterns for the PropertyName production.
var propertyName = [
    // PropertyName :: LiteralPropertyName :: IdentifierName
    "\\w+",

    // PropertyName :: LiteralPropertyName :: StringLiteral
    "(?:'[^']*')",
    "(?:\"[^\"]*\")",

    // PropertyName :: LiteralPropertyName :: NumericLiteral
    "\\d+",

    // PropertyName :: ComputedPropertyName
    "(?:\\[[^\\]]+\\])",
].join("|")

var nativeCode = RegExp([
    "^", "function", ("(" + propertyName + ")?"), "\\(", "\\)", "\\{", "\\[native code\\]", "\\}", "$"
].join("\\s*"));

function assertFunctionName(fun, expected) {
    var match = nativeCode.exec(fun.toString());
    assertEq(match[1], expected);
}


// Function declarations.

eval(`
function funDecl() {}
function* genDecl() {}
async function asyncFunDecl() {}
async function* asyncGenDecl() {}
`);

assertFunctionName(funDecl, "funDecl");
assertFunctionName(genDecl, "genDecl");
assertFunctionName(asyncFunDecl, "asyncFunDecl");
assertFunctionName(asyncGenDecl, "asyncGenDecl");


// Named function expressions.

eval(`
var funExpr = function fn() {};
var genExpr = function* fn() {};
var asyncFunExpr = async function fn() {};
var asyncGenExpr = async function* fn() {};
`);

assertFunctionName(funExpr, "fn");
assertFunctionName(genExpr, "fn");
assertFunctionName(asyncFunExpr, "fn");
assertFunctionName(asyncGenExpr, "fn");


// Anonymous function expressions.

eval(`
var funExprAnon = function() {};
var genExprAnon = function*() {};
var asyncFunExprAnon = async function() {};
var asyncGenExprAnon = async function*() {};
`);

assertFunctionName(funExprAnon, undefined);
assertFunctionName(genExprAnon, undefined);
assertFunctionName(asyncFunExprAnon, undefined);
assertFunctionName(asyncGenExprAnon, undefined);


// Class declarations and expressions (implicit constructor).
eval(`
class classDecl {}
var classExpr = class C {};
var classExprAnon = class {};
var classExprAnonField = class {x = 1};

this.classDecl = classDecl;
`);

assertFunctionName(classDecl, "classDecl");
assertFunctionName(classExpr, "C");
assertFunctionName(classExprAnon, undefined);
assertFunctionName(classExprAnonField, undefined);


// Class declarations and expressions (explicit constructor).
eval(`
class classDecl { constructor() {} }
var classExpr = class C { constructor() {} };
var classExprAnon = class { constructor() {} };

this.classDecl = classDecl;
`);

assertFunctionName(classDecl, "classDecl");
assertFunctionName(classExpr, "C");
assertFunctionName(classExprAnon, undefined);


// Method definitions (identifier names).
eval(`
var obj = {
   m() {},
   *gm() {},
   async am() {},
   async* agm() {},
   get x() {},
   set x(_) {},
};
`);

assertFunctionName(obj.m, undefined);
assertFunctionName(obj.gm, undefined);
assertFunctionName(obj.am, undefined);
assertFunctionName(obj.agm, undefined);
assertFunctionName(Object.getOwnPropertyDescriptor(obj, "x").get, undefined);
assertFunctionName(Object.getOwnPropertyDescriptor(obj, "x").set, undefined);


// Method definitions (string names).
eval(`
var obj = {
    "foo m"() {},
    * "foo gm"() {},
    async "foo am"() {},
    async* "foo agm"() {},
    get "foo x"() {},
    set "foo x"(_) {},
};
`);

assertFunctionName(obj["foo m"], undefined);
assertFunctionName(obj["foo gm"], undefined);
assertFunctionName(obj["foo am"], undefined);
assertFunctionName(obj["foo agm"], undefined);
assertFunctionName(Object.getOwnPropertyDescriptor(obj, "foo x").get, undefined);
assertFunctionName(Object.getOwnPropertyDescriptor(obj, "foo x").set, undefined);


// Method definitions (number names).
eval(`
var obj = {
    100() {},
    *200() {},
    async 300() {},
    async* 400() {},
    get 500() {},
    set 500(_) {},
};
`);

assertFunctionName(obj[100], undefined);
assertFunctionName(obj[200], undefined);
assertFunctionName(obj[300], undefined);
assertFunctionName(obj[400], undefined);
assertFunctionName(Object.getOwnPropertyDescriptor(obj, 500).get, undefined);
assertFunctionName(Object.getOwnPropertyDescriptor(obj, 500).set, undefined);


// Method definitions (computed property names).

var sym1 = Symbol();
var sym2 = Symbol("desc");
var sym3 = Symbol.for("reg-sym");
var sym4 = Symbol.match;
var sym5 = Symbol();

eval(`
var obj = {
    [sym1]() {},
    *[sym2]() {},
    async [sym3]() {},
    async* [sym4]() {},
    get [sym5]() {},
    set [sym5](_) {},
};
`);

assertFunctionName(obj[sym1], undefined);
assertFunctionName(obj[sym2], undefined);
assertFunctionName(obj[sym3], undefined);
assertFunctionName(obj[sym4], undefined);
assertFunctionName(Object.getOwnPropertyDescriptor(obj, sym5).get, undefined);
assertFunctionName(Object.getOwnPropertyDescriptor(obj, sym5).set, undefined);


// Arrow functions.
eval(`
var arrowFn = () => {};
var asyncArrowFn = () => {};
`);

assertFunctionName(arrowFn, undefined);
assertFunctionName(asyncArrowFn, undefined);


// asm.js functions.
eval(`
function asm() {
  "use asm";
  function f(){ return 0|0; }
  return {f: f};
}
`);

assertFunctionName(asm, "asm");
assertFunctionName(asm().f, "f");
}

var g = newGlobal({ discardSource: true });
g.evaluate(test.toString() + "test()");
