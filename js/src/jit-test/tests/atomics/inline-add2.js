// |jit-test| slow;
//
// Like inline-add, but with SharedUint32Array, which is a special
// case because the value is representable only as a Number.
// All this tests is that the Uint32 path is being triggered.
//
// This is intended to be run manually with IONFLAGS=logs and
// postprocessing by iongraph to verify manually (by inspecting the
// MIR) that:
//
//  - the add operation is inlined as it should be, with
//    a return type 'Double'
//  - loads and stores are not moved across the add
//
// Be sure to run with --ion-eager --ion-offthread-compile=off.

function add(ta) {
    return Atomics.add(ta, 86, 6);
}

if (!this.SharedArrayBuffer || !this.Atomics || !this.SharedUint32Array)
    quit(0);

var sab = new SharedArrayBuffer(4096);
var ia = new SharedUint32Array(sab);
for ( var i=0, limit=ia.length ; i < limit ; i++ )
    ia[i] = 0xdeadbeef;		// Important: Not an int32-capable value
var v = 0;
for ( var i=0 ; i < 1000 ; i++ )
    v += add(ia);
//print(v);
