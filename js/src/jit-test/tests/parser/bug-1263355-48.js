// |jit-test| skip-if: helperThreadCount() === 0

function eval(source) {
    offThreadCompileModuleToStencil(source);
    let stencil = finishOffThreadCompileModuleToStencil();
    let m = instantiateModuleStencil(stencil);
    moduleLink(m);
    return moduleEvaluate(m);
}
function runTestCase(testcase) {
    if (testcase() !== true) {}
}
eval(`
  function testcase() {
    function set () {}
  }
  runTestCase(testcase);
`);
