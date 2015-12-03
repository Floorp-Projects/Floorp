if (!this.SharedArrayBuffer)
    quit(0);

// This would hit an assertion in debug builds due to an incorrect
// type guard in the code that copies data from STA to TA.

// Original test case

var sab = new SharedArrayBuffer(4);

var x = new Int32Array(sab);
x.__proto__ = (function(){});
new Uint8Array(x);		// Would assert here

// Supposedly equivalent test case, provoking the error directly

var x = new Int32Array(sab);
Object.defineProperty(x, "length", { value: 0 });
new Uint8Array(x);		// Would assert here

// Derived test case - would not tickle the bug, though.

var x = new Int32Array(sab);
Object.defineProperty(x, "length", { value: 1 << 20 });
new Uint8Array(x);
