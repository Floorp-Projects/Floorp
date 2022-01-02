// Check that Script.getEffectfulOffsets behaves sensibly.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var numEffectfulOperations;

function onNewScript(script) {
  script.getChildScripts().forEach(onNewScript);
  numEffectfulOperations += script.getEffectfulOffsets().length;
}
dbg.onNewScript = onNewScript;

function test(code, expected) {
  numEffectfulOperations = 0;
  g.eval(`
function f(a, b, c) {
${code}
}
`);
  assertEq(numEffectfulOperations, expected);
}

test("a.f = 0", 2);
test("a.f++", 2);
test("return a.f", 1);
test("a[b] = 0", 2);
test("a[b]++", 2);
test("return a[b]", 1);
test("a = 0", 1);
test("d = 0", 2);
test("with (a) { b = 0; }", 4);
test("let o = {}; ({x: o.x} = { x: 10 })", 2);
test("var x", 1);
