if (helperThreadCount() === 0)
    quit(0);

var lfGlobal = newGlobal();
lfGlobal.offThreadCompileScript(`{ let x; throw 42; }`);
try {
    lfGlobal.runOffThreadScript();
} catch (e) {
}

lfGlobal.offThreadCompileScript(`function f() { { let x = 42; return x; } }`);
try {
    lfGlobal.runOffThreadScript();
} catch (e) {
}
