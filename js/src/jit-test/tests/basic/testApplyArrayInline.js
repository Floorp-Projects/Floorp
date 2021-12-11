// |jit-test| skip-if: !this.getJitCompilerOptions() || !getJitCompilerOptions()['ion.enable']

// Test inlining in Ion of fun.apply(..., array).

setJitCompilerOption("ion.warmup.trigger", 50);
setJitCompilerOption("offthread-compilation.enable", 0);
gcPreserveCode();

var itercount = 1000;
var warmup = 100;

// Force Ion to do something predictable without having to wait
// forever for it.

if (getJitCompilerOptions()["ion.warmup.trigger"] > warmup)
    setJitCompilerOption("ion.warmup.trigger", warmup);

setJitCompilerOption("offthread-compilation.enable", 0);

function g(a, b, c, d) {
    return a + b + c + (d === undefined);
}

var g_inIonInLoop = false;
var g_inIonAtEnd = false;

function f(xs) {
    var sum = 0;
    var inIonInLoop = 0;
    for ( var i=0 ; i < itercount ; i++ ) {
	inIonInLoop |= inIon();
	sum += g.apply(null, xs);
    }
    g_ionAtEnd = inIon();
    g_inIonInLoop = !!inIonInLoop;
    return sum;
}

// Basic test

assertEq(f([1,2,3,4]), 6*itercount);

// Attempt to detect a botched optimization: either we ion-compiled
// the loop, or we did not ion-compile the function (ion not actually
// effective at all, this can happen).

assertEq(g_inIonInLoop || !g_inIonAtEnd, true);

// If Ion is inert just leave.

if (!g_inIonInLoop) {
    print("Leaving early - ion not kicking in at all");
    quit(0);
}

// Test that we get the correct argument value even if the array has
// fewer initialized members than its length.

var headroom = [1,2,3];
headroom.length = 13;
assertEq(f(headroom), 7*itercount);

// Test that we throw when the array is too long.

var thrown = false;
try {
    var long = [];
    long.length = getMaxArgs() + 1;
    f(long);
}
catch (e) {
    thrown = true;
    assertEq(e instanceof RangeError, true);
}
assertEq(thrown, true);
