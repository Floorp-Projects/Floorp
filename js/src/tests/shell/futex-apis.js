// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!(this.SharedArrayBuffer && this.Atomics)) {
    reportCompare(true,true);
    quit(0);
}

// Checks for parameter validation of wait/wake API.  All of these test
// cases should throw exceptions during parameter validation, before
// we check whether any waiting should be done.

let ab = new ArrayBuffer(16);
let sab = new SharedArrayBuffer(16);

//////////////////////////////////////////////////////////////////////
//
// The view must be an Int32Array on a SharedArrayBuffer.

// Check against non-TypedArray cases.

{
    let values = [null,
		  undefined,
		  true,
		  false,
		  new Boolean(true),
		  10,
		  3.14,
		  new Number(4),
		  "Hi there",
		  new Date,
		  /a*utomaton/g,
		  { password: "qumquat" },
		  new DataView(new ArrayBuffer(10)),
		  new ArrayBuffer(128),
		  new SharedArrayBuffer(128),
		  new Error("Ouch"),
		  [1,1,2,3,5,8],
		  ((x) => -x),
		  new Map(),
		  new Set(),
		  new WeakMap(),
		  new WeakSet(),
		  this.Promise ? new Promise(() => "done") : null,
		  Symbol("halleluja"),
		  // TODO: Proxy?
		  Object,
		  Int32Array,
		  Date,
		  Math,
		  Atomics ];

    for ( let i=0 ; i < values.length ; i++ ) {
	let view = values[i];
	assertThrowsInstanceOf(() => Atomics.wait(view, 0, 0), TypeError);
	assertThrowsInstanceOf(() => Atomics.wake(view, 0), TypeError);
    }
}

// Check against TypedArray on non-shared memory cases.

{
    let views = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array];

    for ( let View of views ) {
	let view = new View(ab);

	assertThrowsInstanceOf(() => Atomics.wait(view, 0, 0), TypeError);
	assertThrowsInstanceOf(() => Atomics.wake(view, 0), TypeError);
    }
}

// Check against TypedArray on shared memory, but wrong view type

{
    let views = [Int8Array, Uint8Array, Int16Array, Uint16Array, Uint32Array,
		 Uint8ClampedArray, Float32Array, Float64Array];

    for ( let View of views ) {
	let view = new View(sab);

	assertThrowsInstanceOf(() => Atomics.wait(view, 0, 0), TypeError);
	assertThrowsInstanceOf(() => Atomics.wake(view, 0), TypeError);
    }
}

//////////////////////////////////////////////////////////////////////
//
// The indices must be in the range of the array

{
    let view = new Int32Array(sab);

    let indices = [ (view) => -1,
		    (view) => view.length,
		    (view) => view.length*2,
		    (view) => undefined,
		    (view) => '3.5',
		    (view) => { password: "qumquat" } ];

    for ( let iidx=0 ; iidx < indices.length ; iidx++ ) {
	let Idx = indices[iidx](view);
	assertThrowsInstanceOf(() => Atomics.wait(view, Idx, 10), RangeError);
	assertThrowsInstanceOf(() => Atomics.wake(view, Idx), RangeError);
    }
}

reportCompare(true,true);
