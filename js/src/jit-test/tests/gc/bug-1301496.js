// |jit-test| skip-if: helperThreadCount() === 0
gczeal(0);
startgc(1, 'shrinking');
offThreadCompileToStencil("");
// Adapted from randomly chosen test: js/src/jit-test/tests/parser/bug-1263355-13.js
gczeal(9);
newGlobal();
