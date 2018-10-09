// |jit-test| slow; skip-if: !('oomTest' in this)

enableGeckoProfiling();
var lfGlobal = newGlobal();
for (lfLocal in this) {
    lfGlobal[lfLocal] = this[lfLocal];
}
const script = 'oomTest(() => getBacktrace({args: true, locals: "123795", thisprops: true}));';
lfGlobal.offThreadCompileScript(script);
lfGlobal.runOffThreadScript();
