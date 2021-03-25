//|jit-test| --ion-pruning=on

// Verify that we can inline ArrayIteratorNext,
// and scalar-replace the result object.

if (getJitCompilerOptions()["ion.warmup.trigger"] <= 150)
    setJitCompilerOption("ion.warmup.trigger", 150);

gczeal(0);

function foo(arr) {
    var iterator = arr[Symbol.iterator]();
    while (true) {
	var result = iterator.next();
	trialInline();
	assertRecoveredOnBailout(result, true);
	if (result.done) {
	    break;
	}
    }
}

with ({}) {}
foo(Array(1000));
