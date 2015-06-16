// Basic getOwnPropertSymbols tests.

var g = newGlobal();
var dbg = Debugger();
var gobj = dbg.addDebuggee(g);

function test(code) {
    code = "(" + code + ");";
    var expected = Object.getOwnPropertySymbols(eval(code));
    g.eval("obj = " + code);
    var actual = gobj.getOwnPropertyDescriptor("obj").value.getOwnPropertySymbols();

    assertEq(JSON.stringify(actual.map((x) => x.toString()).sort()),
             JSON.stringify(expected.map((x) => x.toString()).sort()));
}

test("{}");
test("Array.prototype"); // Symbol.iterator
test("Object.create(null)");
test("(function() {let x = Symbol(); let o = {}; o[x] = 1; return o;})()");
test("(function() {let x = Symbol('foo'); let o = {}; o[x] = 1; return o;})()");
test("(function() {let x = Symbol('foo'); let y = Symbol('bar'); \
                   let o = {}; o[x] = 1; o[y] = 2; return o;})()");
test("(function() {let x = Symbol('foo with spaces'); \
      let o = {}; o[x] = 1; return o;})()");
test("(function() {let x = Symbol('foo'); \
      let o = function(){}; o[x] = 1; return o;})()");
test("(function() {let x = Symbol('foo'); \
      let o = Object.create(null); o[x] = 1; return o;})()");
test("(function() {let x = Symbol('foo'); \
      let o = new Array(); o[x] = 1; return o;})()");
test("(function() {let x = Symbol('foo'); \
      let o = {}; o[x] = 1; delete o[x]; return o;})()");
