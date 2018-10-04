if (helperThreadCount() == 0)
  quit();

function eval(source) {
    offThreadCompileModule(source);
    let m = finishOffThreadModule();
    m.declarationInstantiation();
    return m.evaluation();
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
