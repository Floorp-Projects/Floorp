// |jit-test| skip-if: helperThreadCount() === 0

load(libdir + "asserts.js")

// Don't assert.
offThreadCompileModule("export { x };");
assertThrowsInstanceOf(() => finishOffThreadModule(), SyntaxError);
