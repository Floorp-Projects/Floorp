// Let a few threads hammer on memory with atomics to provoke errors
// in exclusion work.  This test is not 100% fail-safe: the test may
// pass despite a bug, but this is unlikely.

if (!(this.SharedArrayBuffer && this.getSharedArrayBuffer && this.setSharedArrayBuffer))
    quit(0);

// Map an Int32Array on shared memory.  The first location is used as
// a counter, each worker counts up on exit and the main thread will
// wait until the counter reaches the number of workers.  The other
// elements are contended accumulators where we count up and down very
// rapidly and for a long time, any failure in mutual exclusion should
// lead to errors in the result.  (For example, the test fails almost
// immediately when I disable simulation of mutual exclusion in the
// ARM simulator.)

const numWorkers = 4;           // You're not meant to change this
const iterCount = 255;          // Nor this
const sabLength = 1024;         // Nor this

const oddResult = (function () {
    var v = 0;
    for ( var j=0 ; j < numWorkers ; j++ )
        v |= (iterCount << (8 * j));
    return v;
})();

const evenResult = 0;

const sab = new SharedArrayBuffer(sabLength);

setSharedArrayBuffer(sab);

const iab = new SharedInt32Array(sab);

function testRun(limit) {
    console.log("Limit = " + limit);

    // Fork off workers to hammer on memory.
    for ( var i=0 ; i < numWorkers ; i++ ) {
        evalInWorker(`
                     const iab = new SharedInt32Array(getSharedArrayBuffer());
                     const v = 1 << (8 * ${i});
                     for ( var i=0 ; i < ${limit} ; i++ ) {
                         for ( var k=0 ; k < ${iterCount} ; k++ ) {
                             if (i & 1) {
                                 for ( var j=1 ; j < iab.length ; j++ )
                                     Atomics.sub(iab, j, v);
                             }
                             else {
                                 for ( var j=1 ; j < iab.length ; j++ )
                                     Atomics.add(iab, j, v);
                             }
                         }
                     }
                     Atomics.add(iab, 0, 1);
                     `);
    }

    // Wait...
    while (Atomics.load(iab, 0) != numWorkers)
        ;
    Atomics.store(iab, 0, 0);

    // Check the results and clear the array again.
    const v = (limit & 1) ? oddResult : evenResult;
    for ( var i=1 ; i < iab.length ; i++ ) {
        assertEq(iab[i], v);
        iab[i] = 0;
    }
}

function fib(n) {
    if (n < 2)
        return n;
    return fib(n-1) + fib(n-2);
}

const limit = (function () {
    var val = 5;
    if (typeof getJitCompilerOptions == "function") {
        var opt = getJitCompilerOptions();
        if (opt["ion.enable"])
            val = 10;
    }
    return val;
})();

console.log("This test can easily take 60s on 2015-class x86 hardware");
for ( var i=2 ; i < limit ; i++ )
    testRun(fib(i));
