// Script.prototype.startColumn returns the correct column for all scripts.

const g = newGlobal({newCompartment: true, useWindowProxy: true});
const dbg = Debugger(g);
const gw = dbg.addDebuggee(g);

function test(f, expected) {
    const fw = gw.makeDebuggeeValue(f);
    assertEq(fw.callable, true);
    assertEq(fw.script.startColumn, expected);
}

g.eval(`
function f1() { }
`);
test(g.f1, 11);

g.eval(`
var f2 = function({ a, b, c }, d, e, ...more) { };
`);
test(g.f2, 17);

g.eval(`
var f3 = function *() { };
`);
test(g.f3, 19);

g.eval(`
var f4 = async function
  () { };
`);
test(g.f4, 2);

g.eval(`
var f5 = (a, b) => a + b;
`);
test(g.f5, 9);

g.eval(`
var f6 = a => a + 1;
`);
test(g.f6, 9);

g.eval(`
var MyClass = class {
    method() { }
};
var myInstance = new MyClass();
`);
test(g.myInstance.method, 10);
test(g.myInstance.constructor, 14);

const g2 = newGlobal({newCompartment: true, useWindowProxy: true});
const dbg2 = Debugger(g2);
const g2Wrapped = dbg2.addDebuggee(g2);
g2.evaluate(`
function f7() { }
`, {
  forceFullParse: true,
});
const f7w = g2Wrapped.makeDebuggeeValue(g2.f7);
assertEq(f7w.callable, true);
assertEq(f7w.script.startColumn, 11);

g.eval(`
function f8() {
    return function f8Inner() { }
}
`);
test(g.f8, 11);
test(g.f8(), 27);

g.eval(`
var f9 = new Function(\"\");
`);
test(g.f9, 0);

let hit = 0;
let column;
dbg.onDebuggerStatement = function (frame) {
    column = frame.script.startColumn;
    hit += 1;
};

g.eval(`    debugger;`);
assertEq(column, 0);
assertEq(hit, 1);

const location = { fileName: "column.js", lineNumber: 1, columnNumber: 1 };
hit = 0;
g.evaluate(`    debugger;`, location);
assertEq(column, 1);
assertEq(hit, 1);

g.evaluate(`var f10 = function () { };`, location);
test(g.f10, 20);

g.evaluate(`
var f11 = function () { };
`, location);
test(g.f11, 19);
