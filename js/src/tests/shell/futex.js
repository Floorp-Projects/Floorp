// |reftest| slow skip-if(!xulRuntime.shell)
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var DEBUG = false;

function dprint(s) {
    if (DEBUG) print(s);
}

var hasSharedArrayBuffer = !!(this.SharedArrayBuffer &&
                              this.getSharedObject &&
                              this.setSharedObject);

// Futex test

// Only run if helper threads are available.
if (hasSharedArrayBuffer && helperThreadCount() !== 0) {

var mem = new Int32Array(new SharedArrayBuffer(1024));

////////////////////////////////////////////////////////////

// wait() returns "not-equal" if the value is not the expected one.

mem[0] = 42;

assertEq(Atomics.wait(mem, 0, 33), "not-equal");

// wait() returns "timed-out" if it times out

assertEq(Atomics.wait(mem, 0, 42, 100), "timed-out");

////////////////////////////////////////////////////////////

// Main is sharing the buffer with the worker; the worker is clearing
// the buffer.

mem[0] = 42;
mem[1] = 37;
mem[2] = DEBUG;

setSharedObject(mem.buffer);

evalInWorker(`
var mem = new Int32Array(getSharedObject());
function dprint(s) {
    if (mem[2]) print(s);
}
assertEq(mem[0], 42);		// what was written in the main thread
assertEq(mem[1], 37);		//   is read in the worker
mem[1] = 1337;
dprint("Sleeping for 2 seconds");
sleep(2);
dprint("Waking the main thread now");
setSharedObject(null);
assertEq(Atomics.notify(mem, 0, 1), 1); // Can fail spuriously but very unlikely
`);

var then = Date.now();
assertEq(Atomics.wait(mem, 0, 42), "ok");
dprint("Woke up as I should have in " + (Date.now() - then)/1000 + "s");
assertEq(mem[1], 1337); // what was written in the worker is read in the main thread
assertEq(getSharedObject(), null); // The worker's clearing of the mbx is visible

////////////////////////////////////////////////////////////

// Test the default argument to Atomics.notify()

setSharedObject(mem.buffer);

evalInWorker(`
var mem = new Int32Array(getSharedObject());
sleep(2);				// Probably long enough to avoid a spurious error next
assertEq(Atomics.notify(mem, 0), 1);	// Last argument to notify should default to +Infinity
`);

var then = Date.now();
dprint("Main thread waiting on wakeup (2s)");
assertEq(Atomics.wait(mem, 0, 42), "ok");
dprint("Woke up as I should have in " + (Date.now() - then)/1000 + "s");

////////////////////////////////////////////////////////////

// A tricky case: while in the wait there will be an interrupt, and in
// the interrupt handler we will execute a wait.  This is
// explicitly prohibited (for now), so there should be a catchable exception.

var exn = false;
timeout(2, function () {
    dprint("In the interrupt, starting inner wait with timeout 2s");
    try {
        Atomics.wait(mem, 0, 42); // Should throw
    } catch (e) {
        dprint("Got the interrupt exception!");
        exn = true;
    }
    return true;
});
try {
    dprint("Starting outer wait");
    assertEq(Atomics.wait(mem, 0, 42, 5000), "timed-out");
}
finally {
    timeout(-1);
}
assertEq(exn, true);

////////////////////////////////////////////////////////////

} // if (hasSharedArrayBuffer && helperThreadCount() !== 0) { ... }

dprint("Done");
reportCompare(true,true);
