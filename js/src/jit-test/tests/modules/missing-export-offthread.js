if (helperThreadCount() == 0)
    quit();

load(libdir + "asserts.js")

// Don't assert.
offThreadCompileModule("export { x };");
assertThrowsInstanceOf(() => finishOffThreadModule(), SyntaxError);
