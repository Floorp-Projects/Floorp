// |jit-test| --more-compartments;
let lfCode = `
    var g = newGlobal();
    g.debuggeeGlobal = this;
    g.eval("(" + function () {
        dbg = new Debugger(debuggeeGlobal);
        dbg.onExceptionUnwind = function (frame, exc) {};
    } + ")();");
`;
loadFile(lfCode);
// use "await" so the module is marked as TLA
loadFile(lfCode + " await ''");
async function loadFile(lfVarx) {
    try {
        try { evaluate(lfVarx); } catch(exc) {}
        let lfMod = parseModule(lfVarx);
        lfMomoduleLink(d);
        await lfMomoduleEvaluate(d);
    } catch (lfVare) {}
}
