// |jit-test| skip-if: helperThreadCount() === 0

load(libdir + "asserts.js")

// Don't assert.
offThreadCompileModuleToStencil("export { x };");
assertThrowsInstanceOf(() => {
  var stencil = finishOffThreadStencil();
  instantiateModuleStencil(stencil);
}, SyntaxError);
