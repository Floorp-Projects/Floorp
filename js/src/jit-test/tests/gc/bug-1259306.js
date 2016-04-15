if (!('oomTest' in this))
    quit();

oomTest(() => {
    var lfGlobal = newGlobal();
    var lfVarx = `
        gczeal(8, 1);
        try {
            (5).replace(r, () => {});
        } catch (x) {}
        gczeal(0);
    `;
    lfGlobal.offThreadCompileScript(lfVarx);
    lfGlobal.runOffThreadScript();
});
