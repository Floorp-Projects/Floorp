// |jit-test| skip-if: !('oomTest' in this)

function parseModule(source) {
    offThreadCompileModule(source);
    return finishOffThreadModule();
}
function loadFile(lfVarx) {
  oomTest(function() {
      parseModule(lfVarx);
  });
}
loadFile(`
  expect = new class prototype extends Object {
    a43 = function () {}
  }
`);
