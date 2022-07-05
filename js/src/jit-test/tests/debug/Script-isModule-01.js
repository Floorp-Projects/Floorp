// Debugger.Object.prototype.isModule

const g = newGlobal({newCompartment: true});
const dbg = Debugger(g);
let count = 0;
dbg.onNewScript = function (script) {
    count += 1;
    assertEq(script.isModule, true);
};
const m = g.parseModule("");
moduleLink(m);
moduleEvaluate(m);
assertEq(count, 1);
