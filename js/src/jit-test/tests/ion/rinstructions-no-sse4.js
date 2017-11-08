// |jit-test| --no-sse4;

// This test is a fork of dce-with-rinstructions.js. It tests recover
// instructions which are only executed on pre-SSE4 processors.

setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 20);

const max = 200;

// Check that we are able to remove the operation inside recover test
// functions (denoted by "rop..."), when we inline the first version
// of uceFault, and ensure that the bailout is correct when uceFault
// is replaced (which cause an invalidation bailout)
let uceFault = function (i) {
    if (i > 98)
        uceFault = function (i) { return true; };
    return false;
};

let uceFault_floor_double = eval(
    uneval(uceFault)
        .replace('uceFault', 'uceFault_floor_double')
);
function rfloor_double(i) {
    const x = Math.floor(i + (-1 >>> 0));
    if (uceFault_floor_double(i) || uceFault_floor_double(i))
        assertEq(x, 99 + (-1 >>> 0)); /* = i + 2 ^ 32 - 1 */
    assertRecoveredOnBailout(x, true);
    return i;
}

for (let j = 100 - max; j < 100; j++) {
    with({}){} // Do not Ion-compile this loop.
    const i = j < 2 ? (Math.abs(j) % 50) + 2 : j;
    rfloor_double(i);
}
