// Debugger.Object.prototype.isModule

const g = newGlobal({newCompartment: true});
const dbg = Debugger(g);
let count = 0;
dbg.onNewScript = function (script) {
    count += 1;
    assertEq(script.isModule, true);

    dbg.onNewScript = function (script) {
        count += 1;
        assertEq(script.isModule, false);
    };
};
const m = g.parseModule("eval('')");
moduleLink(m);
moduleEvaluate(m);
assertEq(count, 2);
