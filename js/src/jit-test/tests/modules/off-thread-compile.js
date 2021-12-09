// |jit-test| skip-if: helperThreadCount() === 0

// Test off thread module compilation.

load(libdir + "asserts.js");

function offThreadParseAndEvaluate(source) {
    offThreadCompileModuleToStencil(source);
    let stencil = finishOffThreadCompileModuleToStencil();
    let m = instantiateModuleStencil(stencil);
    m.declarationInstantiation();
    return m.evaluation();
}

offThreadParseAndEvaluate("export let x = 2 * 3;");
