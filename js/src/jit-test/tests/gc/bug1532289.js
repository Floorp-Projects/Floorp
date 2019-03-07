// |jit-test| --ion-warmup-threshold=0; --ion-offthread-compile=off; skip-if: !getJitCompilerOptions()['baseline.enable']

// gczeal mode causes test to run too slowly with --no-baseline
gczeal(4,40);

var x;
var y = false;

function f(v) { x = v; while (y) {} }

for (var z=1; z < 1e5; z++) { f(BigInt(z)); }
