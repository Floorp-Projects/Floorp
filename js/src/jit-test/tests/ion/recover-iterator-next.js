// |jit-test| test-join=--no-unboxed-objects
//
// Unboxed object optimization might not trigger in all cases, thus we ensure
// that Scalar Replacement optimization is working well independently of the
// object representation.

// Ion eager fails the test below because we have not yet created any
// template object in baseline before running the content of the top-level
// function.
if (getJitCompilerOptions()["ion.warmup.trigger"] <= 90)
    setJitCompilerOption("ion.warmup.trigger", 90);

// This test checks that we are able to remove the getprop & setprop with scalar
// replacement, so we should not force inline caches, as this would skip the
// generation of getprop & setprop instructions.
if (getJitCompilerOptions()["ion.forceinlineCaches"])
    setJitCompilerOption("ion.forceinlineCaches", 0);

var arr = new Array();
var max = 2000;
for (var i=0; i < max; i++)
    arr[i] = i;

function f() {
    var res = 0;
    var nextObj;
    var itr = arr[Symbol.iterator]();
    do {
        nextObj = itr.next();
        if (nextObj.done)
          break;
        res += nextObj.value;
        assertRecoveredOnBailout(nextObj, true);
    } while (true);
    return res;
}

for (var j = 0; j < 10; j++)
  assertEq(f(), max * (max - 1) / 2);
