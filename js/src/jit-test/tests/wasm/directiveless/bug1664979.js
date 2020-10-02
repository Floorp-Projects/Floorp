// |jit-test| --fuzzing-safe; --ion-offthread-compile=off; skip-if: !wasmIsSupported()

var all = [undefined, null, ];
function AsmModule(stdlib) {
    "use asm";
    var fround = stdlib.Math.fround;
    function fltConvNot(y38) {
        y38 = fround(y38);
        var i38 = 0;
        i38 = ~((~~y38) | 0);
        return (!!i38) | 0;
    }
    return {
        fltConvNot: fltConvNot,
    };
}
var asmModule = AsmModule({
    Math: Math
});
for (var i38 = 0; i38 < 10; ++i38)
  asmModule.fltConvNot(all[i38])
