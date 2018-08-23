// Test off thread module compilation.

if (helperThreadCount() == 0)
    quit();

load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

function offThreadParseAndEvaluate(source) {
    offThreadCompileModule(source);
    let m = finishOffThreadModule();
    instantiateModule(m);
    return evaluateModule(m);
}

offThreadParseAndEvaluate("export let x = 2 * 3;");
