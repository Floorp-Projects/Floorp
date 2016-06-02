gczeal(15,5);
try {
    foobar();
} catch (e) {}
function newFunc(x) {
    new Function(x)();
};
loadFile(`
  try { gczeal(10, 2)() } catch (e) {}
`);
function loadFile(lfVarx) {
    function newFunc(x) {
        new Function(x)();
    };
    newFunc(lfVarx);
    if (helperThreadCount() && getJitCompilerOptions()["offthread-compilation.enable"]) {}
}
