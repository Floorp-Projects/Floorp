// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!(this.SharedArrayBuffer && this.getSharedArrayBuffer && this.setSharedArrayBuffer)) {
    reportCompare(true,true);
    quit(0);
}

var DEBUG = false;

function dprint(s) {
    if (DEBUG) print(s);
}

// Tests the SharedArrayBuffer mailbox in the shell.
// Tests the wait/wake functionality in the shell.

var sab = new SharedArrayBuffer(12);
var mem = new Int32Array(sab);

// SharedArrayBuffer mailbox tests

assertEq(getSharedArrayBuffer(), null); // Mbx starts empty

assertEq(setSharedArrayBuffer(mem.buffer), undefined); // Setter returns undefined
assertEq(getSharedArrayBuffer() == null, false);       // And then the mbx is not empty

var v = getSharedArrayBuffer();
assertEq(v.byteLength, mem.buffer.byteLength); // Looks like what we put in?
var w = new Int32Array(v);
mem[0] = 314159;
assertEq(w[0], 314159);		// Shares memory (locally) with what we put in?
mem[0] = 0;

setSharedArrayBuffer(null);	// Setting to null clears to null
assertEq(getSharedArrayBuffer(), null);

setSharedArrayBuffer(mem.buffer);
setSharedArrayBuffer(undefined); // Setting to undefined clears to null
assertEq(getSharedArrayBuffer(), null);

setSharedArrayBuffer(mem.buffer);
setSharedArrayBuffer();		// Setting without arguments clears to null
assertEq(getSharedArrayBuffer(), null);

// Only SharedArrayBuffer can be stored in the mbx

assertThrowsInstanceOf(() => setSharedArrayBuffer({x:10, y:20}), Error);
assertThrowsInstanceOf(() => setSharedArrayBuffer([1,2]), Error);
assertThrowsInstanceOf(() => setSharedArrayBuffer(new ArrayBuffer(10)), Error);
assertThrowsInstanceOf(() => setSharedArrayBuffer(new Int32Array(10)), Error);
assertThrowsInstanceOf(() => setSharedArrayBuffer(false), Error);
assertThrowsInstanceOf(() => setSharedArrayBuffer(3.14), Error);
assertThrowsInstanceOf(() => setSharedArrayBuffer(mem), Error);
assertThrowsInstanceOf(() => setSharedArrayBuffer("abracadabra"), Error);
assertThrowsInstanceOf(() => setSharedArrayBuffer(() => 37), Error);

// Futex test

if (helperThreadCount() === 0) {
  // Abort if there is no helper thread.
  reportCompare(true,true);
  quit();
}

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

setSharedArrayBuffer(mem.buffer);

evalInWorker(`
var mem = new Int32Array(getSharedArrayBuffer());
function dprint(s) {
    if (mem[2]) print(s);
}
assertEq(mem[0], 42);		// what was written in the main thread
assertEq(mem[1], 37);		//   is read in the worker
mem[1] = 1337;
dprint("Sleeping for 2 seconds");
sleep(2);
dprint("Waking the main thread now");
setSharedArrayBuffer(null);
assertEq(Atomics.wake(mem, 0, 1), 1); // Can fail spuriously but very unlikely
`);

var then = Date.now();
assertEq(Atomics.wait(mem, 0, 42), "ok");
dprint("Woke up as I should have in " + (Date.now() - then)/1000 + "s");
assertEq(mem[1], 1337); // what was written in the worker is read in the main thread
assertEq(getSharedArrayBuffer(), null); // The worker's clearing of the mbx is visible

////////////////////////////////////////////////////////////

// Test the default argument to atomics.wake()

setSharedArrayBuffer(mem.buffer);

evalInWorker(`
var mem = new Int32Array(getSharedArrayBuffer());
sleep(2);				// Probably long enough to avoid a spurious error next
assertEq(Atomics.wake(mem, 0), 1);	// Last argument to wake should default to +Infinity
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

dprint("Done");
reportCompare(true,true);
