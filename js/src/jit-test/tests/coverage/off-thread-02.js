// |jit-test| --code-coverage; --no-ion; skip-if: helperThreadCount() === 0

assertEq(isLcovEnabled(), true);

offThreadCompileModuleToStencil(`
    globalThis.hitCount = 0;
    function offThreadFun() {
        globalThis.hitCount += 1;
    }

    offThreadFun();
    offThreadFun();
    offThreadFun();
    offThreadFun();
`);
let stencil = finishOffThreadStencil();
let mod = instantiateModuleStencil(stencil);
moduleLink(mod);
moduleEvaluate(mod);
assertEq(hitCount, 4);

const expected = "FNDA:4,offThreadFun";

let report = getLcovInfo();
assertEq(report.includes(expected), true);
