// |jit-test| --ion-warmup-threshold=0; --ion-offthread-compile=off

gczeal(4,40);

var x;
var y = false;

function f(v) { x = v; while (y) {} }

for (var z=1; z < 1e5; z++) { f(BigInt(z)); }
