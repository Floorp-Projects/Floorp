setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("ion.warmup.trigger", 120);
gczeal(0);
with ({}) {}

function bar() {
    // |arguments| can be recovered, even if this is inlined into an OSR script.
    assertRecoveredOnBailout(arguments, true);
    return arguments[0];
}

function foo(n) {
    // |arguments| for an OSR script can't be recovered.
    assertRecoveredOnBailout(arguments, false);
    var sum = 0;
    for (var i = 0; i < n; i++) {
	sum += bar(i);
    }
    trialInline();
    return sum;
}

// Trigger trial-inlining of bar into foo.
foo(110);

// OSR-compile foo.
assertEq(foo(1000), 499500);
