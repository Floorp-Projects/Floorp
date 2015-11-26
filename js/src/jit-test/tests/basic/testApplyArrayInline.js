// Test inlining in Ion of fun.apply(..., array).

if (!this.getJitCompilerOptions() || !getJitCompilerOptions()['ion.enable'])
    quit(0);

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

// Test that the optimization is effective.  There's the possibility
// of some false results here, and in release builds the margins are
// actually small.

itercount *= 2;

var A = Date.now();
assertEq(f([1,2,3,4]), 6*itercount)	// Fast path because a sane array
var AinLoop = g_inIonInLoop;
var B = Date.now();
assertEq(f([1,2,3]), 7*itercount);	// Fast path because a sane array, even if short
var BinLoop = g_inIonInLoop;
var C = Date.now();
assertEq(f(headroom), 7*itercount);	// Slow path because length > initializedLength
var CinLoop = g_inIonInLoop;
var D = Date.now();
if (AinLoop && BinLoop && CinLoop) {
    print("No bailout: " + (B - A));
    print("Short: " + (C - B));
    print("Bailout: " + (D - C));
    assertEq((D - C) >= (B - A), true);
    assertEq((D - C) >= (C - B), true);
} else {
    print("Not running perf test");
}
