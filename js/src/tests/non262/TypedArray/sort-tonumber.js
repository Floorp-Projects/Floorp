var BUGNUMBER = 230216;
var summary = 'Ensure ToNumber is called on the result of compareFn inside TypedArray.prototype.sort';

printBugNumber(BUGNUMBER);
printStatus(summary);

var ta = new Int32Array(4);
var ab = ta.buffer;

var called = false;
try {
  ta.sort(function(a, b) {
    // IsDetachedBuffer is checked right after calling the compare function.
    // The order of operations is:
    // var tmp = compareFn(a, b)
    // var res = ToNumber(tmp)
    // if IsDetachedBuffer, throw TypeError
    // [...]
    // inspect `res` to determine sorting (calling ToNumber in the process)
    // So, detach the ArrayBuffer to throw, to make sure we're actually calling ToNumber immediately (as spec'd)
    detachArrayBuffer(ab);
    return {
      [Symbol.toPrimitive]() { called = true; }
    };
  });
} catch (e) { }

if (typeof reportCompare === "function")
    reportCompare(true, called);
