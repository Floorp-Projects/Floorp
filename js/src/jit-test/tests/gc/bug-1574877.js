// |jit-test| skip-if: !('oomTest' in this)

function parseModule(source) {
    offThreadCompileModuleToStencil(source);
    var stencil = finishOffThreadStencil();
    return instantiateModuleStencil(stencil);
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
