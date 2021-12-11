setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("ion.warmup.trigger", 0);

function ceil(a, b) {
    return Math.ceil((a | 0) / (b | 0)) | 0;
}
function floor(a, b) {
    return Math.floor((a | 0) / (b | 0)) | 0;
}
function round(a, b) {
    return Math.round((a | 0) / (b | 0)) | 0;
}
function intdiv(a, b) {
    return ((a | 0) / (b | 0)) | 0;
}

// Always rounds up.
assertEq(ceil(5, 5), 1);
assertEq(ceil(4, 3), 2);
assertEq(ceil(5, 3), 2);
assertEq(ceil(-4, 3), -1);
assertEq(ceil(-5, 3), -1);

// Always rounds down.
assertEq(floor(5, 5), 1);
assertEq(floor(4, 3), 1);
assertEq(floor(5, 3), 1);
assertEq(floor(-4, 3), -2);
assertEq(floor(-5, 3), -2);

// Always rounds towards the nearest.
assertEq(round(5, 5), 1);
assertEq(round(4, 3), 1);
assertEq(round(5, 3), 2);
assertEq(round(-4, 3), -1);
assertEq(round(-5, 3), -2);

// Always rounds towards zero.
assertEq(intdiv(5, 5), 1);
assertEq(intdiv(4, 3), 1);
assertEq(intdiv(5, 3), 1);
assertEq(intdiv(-4, 3), -1);
assertEq(intdiv(-5, 3), -1);
