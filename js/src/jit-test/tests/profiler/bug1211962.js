// |jit-test| slow;
if (!('oomTest' in this))
    quit();

enableGeckoProfiling();
var lfGlobal = newGlobal();
for (lfLocal in this) {
    lfGlobal[lfLocal] = this[lfLocal];
}
const script = 'oomTest(() => getBacktrace({args: true, locals: "123795", thisprops: true}));';
lfGlobal.offThreadCompileScript(script);
lfGlobal.runOffThreadScript();
