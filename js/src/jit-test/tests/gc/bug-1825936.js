let src = `
function f0() {
    return f0;
}
const v10 = f0.bind();
v10.sameZoneAs = f0;
const v37 = this.newGlobal(v10);

try {
    v37.moduleEvaluate();
} catch(e48) {
    this.grayRoot();
    const v59 = new FinalizationRegistry(FinalizationRegistry);
    v59.register(e48, v59, e48);
    v59.register(f0, v59, e48);
}
this.nukeAllCCWs();
`;

gczeal(0);
let global = newGlobal();
global.eval(src);
global = undefined;
gc();
