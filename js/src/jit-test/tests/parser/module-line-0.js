load(libdir + "asserts.js");

compileToStencil("", { lineNumber: 1, module: true });
assertThrowsInstanceOf(() => {
  compileToStencil("", { lineNumber: 0, module: true });
}, Error);

compileToStencilXDR("", { lineNumber: 1, module: true });
assertThrowsInstanceOf(() => {
  compileToStencilXDR("", { lineNumber: 0, module: true });
}, Error);

if (helperThreadCount() > 0) {
  offThreadCompileModuleToStencil("", { lineNumber: 1, module: true });
  assertThrowsInstanceOf(() => {
    offThreadCompileModuleToStencil("", { lineNumber: 0, module: true });
  }, Error);
}
