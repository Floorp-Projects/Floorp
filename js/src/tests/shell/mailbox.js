// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// Tests the shared-object mailbox in the shell.

var hasSharedMemory = !!(this.SharedArrayBuffer &&
                         this.getSharedObject &&
                         this.setSharedObject);

if (!hasSharedMemory) {
    reportCompare(true, true);
    quit(0);
}

var sab = new SharedArrayBuffer(12);
var mem = new Int32Array(sab);

// SharedArrayBuffer mailbox tests

assertEq(getSharedObject(), null); // Mbx starts empty

assertEq(setSharedObject(mem.buffer), undefined); // Setter returns undefined
assertEq(getSharedObject() == null, false);       // And then the mbx is not empty

var v = getSharedObject();
assertEq(v.byteLength, mem.buffer.byteLength); // Looks like what we put in?
var w = new Int32Array(v);
mem[0] = 314159;
assertEq(w[0], 314159);		// Shares memory (locally) with what we put in?
mem[0] = 0;

setSharedObject(3.14);	// Share numbers
assertEq(getSharedObject(), 3.14);

setSharedObject(null);	// Setting to null clears to null
assertEq(getSharedObject(), null);

setSharedObject(mem.buffer);
setSharedObject(undefined); // Setting to undefined clears to null
assertEq(getSharedObject(), null);

setSharedObject(mem.buffer);
setSharedObject();		// Setting without arguments clears to null
assertEq(getSharedObject(), null);

// Non-shared objects cannot be stored in the mbx

assertThrowsInstanceOf(() => setSharedObject({x:10, y:20}), Error);
assertThrowsInstanceOf(() => setSharedObject([1,2]), Error);
assertThrowsInstanceOf(() => setSharedObject(new ArrayBuffer(10)), Error);
assertThrowsInstanceOf(() => setSharedObject(new Int32Array(10)), Error);
assertThrowsInstanceOf(() => setSharedObject(false), Error);
assertThrowsInstanceOf(() => setSharedObject(mem), Error);
assertThrowsInstanceOf(() => setSharedObject("abracadabra"), Error);
assertThrowsInstanceOf(() => setSharedObject(() => 37), Error);

// We can store wasm shared memories, too

if (!this.WebAssembly || !wasmThreadsSupported()) {
    reportCompare(true, true);
    quit(0);
}

setSharedObject(null);

var mem = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
setSharedObject(mem);
var ia1 = new Int32Array(mem.buffer);

var mem2 = getSharedObject();
assertEq(mem2.buffer instanceof SharedArrayBuffer, true);
assertEq(mem2.buffer.byteLength, 65536);
var ia2 = new Int32Array(mem2.buffer);

ia2[37] = 0x12345678;
assertEq(ia1[37], 0x12345678);

// Can't store a non-shared memory

assertThrowsInstanceOf(() => setSharedObject(new WebAssembly.Memory({initial: 1, maximum: 1})), Error);

// We can store wasm modules

var mod = new WebAssembly.Module(wasmTextToBinary(`(module
                                                    (func (export "hi") (result i32)
                                                     (i32.const 37))
                                                    (import "m" "f" (param i32) (result i32)))`));

setSharedObject(mod);

var mod2 = getSharedObject();

// This should fail because we're not providing the correct import object
assertThrowsInstanceOf(() => new WebAssembly.Instance(mod2, {m:{}}), WebAssembly.LinkError);

// But this should work
new WebAssembly.Instance(mod2, {m:{f:(x) => x}});

reportCompare(true,true);
