// |jit-test| slow;
//
// This is intended to be run manually with IONFLAGS=logs and
// postprocessing by iongraph to verify manually (by inspecting the
// MIR) that:
//
//  - the fence operation is inlined as it should be
//  - loads and stores are not moved across the fence
//
// Be sure to run with --ion-eager --ion-offthread-compile=off.

function fence(ta) {
    var x = ta[0];
    Atomics.fence();
    var y = ta[1];
    var z = y + 1;
    var w = x + z;
    return w;
}

if (!this.SharedArrayBuffer || !this.Atomics || !this.SharedInt32Array)
    quit(0);

var sab = new SharedArrayBuffer(4096);
var ia = new SharedInt32Array(sab);
for ( var i=0, limit=ia.length ; i < limit ; i++ )
    ia[i] = 37;
var v = 0;
for ( var i=0 ; i < 1000 ; i++ )
    v += fence(ia);
//print(v);
