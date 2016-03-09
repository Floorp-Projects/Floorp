// Test off thread module compilation.

if (helperThreadCount() == 0)
    quit();

load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

function offThreadParseAndEvaluate(source) {
    offThreadCompileModule(source);
    let m = finishOffThreadModule();
    m.declarationInstantiation();
    return m.evaluation();
}

offThreadParseAndEvaluate("export let x = 2 * 3;");
