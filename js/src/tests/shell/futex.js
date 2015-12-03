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
// Tests the futex functionality in the shell.

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

// Main is sharing the buffer with the worker; the worker is clearing
// the buffer.

mem[0] = 42;
mem[1] = 37;
mem[2] = DEBUG;
setSharedArrayBuffer(mem.buffer);

if (helperThreadCount() === 0) {
  // Abort if there is no helper thread.
  reportCompare(true,true);
  quit();
}

evalInWorker(`
var mem = new Int32Array(getSharedArrayBuffer());
function dprint(s) {
    if (mem[2]) print(s);
}
assertEq(mem[0], 42);		// what was written in the main thread
assertEq(mem[1], 37);		//   is read in the worker
mem[1] = 1337;
dprint("Sleeping for 3 seconds");
sleep(3);
dprint("Waking the main thread now");
setSharedArrayBuffer(null);
Atomics.futexWake(mem, 0, 1);
`);

var then = Date.now();
assertEq(Atomics.futexWait(mem, 0, 42), Atomics.OK);
dprint("Woke up as I should have in " + (Date.now() - then)/1000 + "s");
assertEq(mem[1], 1337); // what was written in the worker is read in the main thread
assertEq(getSharedArrayBuffer(), null); // The worker's clearing of the mbx is visible

// A tricky case: while in the wait there will be an interrupt, and in
// the interrupt handler we will execute a futexWait.  This is
// explicitly prohibited (for now), so there should be a catchable exception.

timeout(2, function () {
    dprint("In the interrupt, starting inner wait");
    Atomics.futexWait(mem, 0, 42); // Should throw and propagate all the way out
});
var exn = false;
try {
    dprint("Starting outer wait");
    assertEq(Atomics.futexWait(mem, 0, 42, 5000), Atomics.OK);
}
catch (e) {
    dprint("Got the exception!");
    exn = true;
}
finally {
    timeout(-1);
}
assertEq(exn, true);
dprint("Done");

reportCompare(true,true);
