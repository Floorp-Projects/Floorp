// |reftest| skip-if(!this.hasOwnProperty("setDiscardSource")) -- uses the setDiscardSource testing function

// The Function.prototype.toString() representation of sourceless functions
// must match the NativeFunction syntax.

setDiscardSource(true);

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
    "^", "function", ("(?:" + propertyName + ")?"), "\\(", "\\)", "\\{", "\\[native code\\]", "\\}", "$"
].join("\\s*"));


// Function declarations.

eval(`
function funDecl() {}
function* genDecl() {}
async function asyncFunDecl() {}
async function* asyncGenDecl() {}
`);

reportMatch(nativeCode, funDecl.toString());
reportMatch(nativeCode, genDecl.toString());
reportMatch(nativeCode, asyncFunDecl.toString());
reportMatch(nativeCode, asyncGenDecl.toString());


// Named function expressions.

eval(`
var funExpr = function fn() {};
var genExpr = function* fn() {};
var asyncFunExpr = async function fn() {};
var asyncGenExpr = async function* fn() {};
`);

reportMatch(nativeCode, funExpr.toString());
reportMatch(nativeCode, genExpr.toString());
reportMatch(nativeCode, asyncFunExpr.toString());
reportMatch(nativeCode, asyncGenExpr.toString());


// Anonymous function expressions.

eval(`
var funExprAnon = function() {};
var genExprAnon = function*() {};
var asyncFunExprAnon = async function() {};
var asyncGenExprAnon = async function*() {};
`);

reportMatch(nativeCode, funExprAnon.toString());
reportMatch(nativeCode, genExprAnon.toString());
reportMatch(nativeCode, asyncFunExprAnon.toString());
reportMatch(nativeCode, asyncGenExprAnon.toString());


// Class declarations and expressions (implicit constructor).
eval(`
class classDecl {}
var classExpr = class C {};
var classExprAnon = class {};

this.classDecl = classDecl;
`);

reportMatch(nativeCode, classDecl.toString());
reportMatch(nativeCode, classExpr.toString());
reportMatch(nativeCode, classExprAnon.toString());


// Class declarations and expressions (explicit constructor).
eval(`
class classDecl { constructor() {} }
var classExpr = class C { constructor() {} };
var classExprAnon = class { constructor() {} };

this.classDecl = classDecl;
`);

reportMatch(nativeCode, classDecl.toString());
reportMatch(nativeCode, classExpr.toString());
reportMatch(nativeCode, classExprAnon.toString());


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

reportMatch(nativeCode, obj.m.toString());
reportMatch(nativeCode, obj.gm.toString());
reportMatch(nativeCode, obj.am.toString());
reportMatch(nativeCode, obj.agm.toString());
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(obj, "x").get.toString());
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(obj, "x").set.toString());


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

reportMatch(nativeCode, obj["foo m"].toString());
reportMatch(nativeCode, obj["foo gm"].toString());
reportMatch(nativeCode, obj["foo am"].toString());
reportMatch(nativeCode, obj["foo agm"].toString());
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(obj, "foo x").get.toString());
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(obj, "foo x").set.toString());


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

reportMatch(nativeCode, obj[100].toString());
reportMatch(nativeCode, obj[200].toString());
reportMatch(nativeCode, obj[300].toString());
reportMatch(nativeCode, obj[400].toString());
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(obj, 500).get.toString());
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(obj, 500).set.toString());


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

reportMatch(nativeCode, obj[sym1].toString());
reportMatch(nativeCode, obj[sym2].toString());
reportMatch(nativeCode, obj[sym3].toString());
reportMatch(nativeCode, obj[sym4].toString());
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(obj, sym5).get.toString());
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(obj, sym5).set.toString());


// Arrow functions.
eval(`
var arrowFn = () => {};
var asyncArrowFn = () => {};
`);

reportMatch(nativeCode, arrowFn.toString());
reportMatch(nativeCode, asyncArrowFn.toString());


// asm.js functions.
eval(`
function asm() {
  "use asm";
  function f(){ return 0|0; }
  return {f: f};
}
`);

reportMatch(nativeCode, asm.toString());
reportMatch(nativeCode, asm().f.toString());
