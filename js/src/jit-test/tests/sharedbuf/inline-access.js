// |jit-test| slow;
//
// This is for testing inlining behavior in the jits.
//
// For Baseline, run:
//    $ IONFLAGS=bl-ic .../js --ion-off --baseline-eager inline-access.js
// Then inspect the output, there should be calls to "GetElem(TypedArray[Int32])",
// "GetProp(NativeObj/NativeGetter 0x...)", and "SetElem_TypedArray stub"
// for the read access, length access, and write access respectively, within f.
//
// For Ion, run:
//    $ IONFLAGS=logs .../js --ion-offthread-compile=off inline-access.js
// Then postprocess with iongraph and verify (by inspecting MIR late in the pipeline)
// that it contains instructions like "typedarraylength", "loadtypedarrayelement",
// and "storetypedarrayelement".

if (!this.SharedArrayBuffer)
    quit();

function f(ta) {
    return (ta[2] = ta[0] + ta[1] + ta.length);
}

var v = new Int32Array(new SharedArrayBuffer(4096));
var sum = 0;
var iter = 1000;
for ( var i=0 ; i < iter ; i++ )
    sum += f(v);
assertEq(sum, v.length * iter);
