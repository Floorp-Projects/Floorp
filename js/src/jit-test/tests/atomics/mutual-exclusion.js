// Let a few threads hammer on memory with atomics to provoke errors
// in exclusion work.  This test is not 100% fail-safe: the test may
// pass despite a bug, but this is unlikely.

if (!(this.SharedArrayBuffer && this.getSharedArrayBuffer && this.setSharedArrayBuffer && this.evalInWorker))
    quit(0);

try {
    // This will fail with --no-threads.
    evalInWorker("37");
}
catch (e) {
    quit(0);
}

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

const iab = new Int32Array(sab);

function testRun(limit) {
    console.log("Limit = " + limit);

    // Fork off workers to hammer on memory.
    for ( var i=0 ; i < numWorkers ; i++ ) {
        evalInWorker(`
                     const iab = new Int32Array(getSharedArrayBuffer());
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

// Under some configurations the test can take a while to run (and may
// saturate the CPU since it runs four workers); try not to time out.

var then = new Date();
testRun(1);
if (new Date() - then < 20000) {
    testRun(2);
    if (new Date() - then < 30000) {
	testRun(3);
    }
}
