load(libdir + "asserts.js");

compileToStencil("", { fileName: "", module: true });
assertThrowsInstanceOf(() => {
  compileToStencil("", { fileName: null, module: true });
}, Error);

if (helperThreadCount() > 0) {
  offThreadCompileModuleToStencil("", { fileName: "", module: true });
  assertThrowsInstanceOf(() => {
    offThreadCompileModuleToStencil("", { fileName: null, module: true });
  }, Error);
}
