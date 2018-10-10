// |jit-test| skip-if: helperThreadCount() === 0

// Test off thread module compilation.

load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

function offThreadParseAndEvaluate(source) {
    offThreadCompileModule(source);
    let m = finishOffThreadModule();
    m.declarationInstantiation();
    return m.evaluation();
}

offThreadParseAndEvaluate("export let x = 2 * 3;");
