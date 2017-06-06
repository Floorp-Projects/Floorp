if (helperThreadCount() == 0)
    quit();
gczeal(0);
startgc(1, 'shrinking');
offThreadCompileScript("");
// Adapted from randomly chosen test: js/src/jit-test/tests/parser/bug-1263355-13.js
gczeal(9);
newGlobal();
