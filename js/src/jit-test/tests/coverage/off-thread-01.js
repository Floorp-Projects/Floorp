// |jit-test| --code-coverage; --no-ion; skip-if: helperThreadCount() === 0

assertEq(isLcovEnabled(), true);

offThreadCompileToStencil(`
    let hitCount = 0;
    function offThreadFun() {
        hitCount += 1;
    }

    offThreadFun();
    offThreadFun();
    offThreadFun();
    offThreadFun();
`);
var stencil = finishOffThreadStencil();
evalStencil(stencil);
assertEq(hitCount, 4);

let report = getLcovInfo();

const expected = "FNDA:4,offThreadFun";
assertEq(report.includes(expected), true);
