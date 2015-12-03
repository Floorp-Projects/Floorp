// |jit-test| slow;
//
// This is intended to be run manually with IONFLAGS=logs and
// postprocessing by iongraph to verify manually (by inspecting the
// MIR) that:
//
//  - the cmpxchg operation is inlined as it should be
//  - loads and stores are not moved across the cmpxchg
//
// Be sure to run with --ion-eager --ion-offthread-compile=off.

function cmpxchg(ta) {
    var x = ta[0];
    Atomics.compareExchange(ta, 86, 37, 42);
    var y = ta[1];
    var z = y + 1;
    var w = x + z;
    return w;
}

if (!this.SharedArrayBuffer || !this.Atomics)
    quit(0);

var sab = new SharedArrayBuffer(4096);
var ia = new Int32Array(sab);
for ( var i=0, limit=ia.length ; i < limit ; i++ )
    ia[i] = 37;
var v = 0;
for ( var i=0 ; i < 1000 ; i++ )
    v += cmpxchg(ia);
//print(v);
