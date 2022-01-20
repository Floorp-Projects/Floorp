// |jit-test| skip-if: helperThreadCount() === 0
for (let x = 0; x < 99; ++x)
    evalInWorker("newGlobal().offThreadCompileToStencil(\"{}\");");
