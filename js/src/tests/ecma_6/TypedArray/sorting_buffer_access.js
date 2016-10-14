// Ensure that when sorting arrays of size greater than 128, which
// calls RadixSort under the hood, we don't access the 'buffer' 
// property of the typed array directly. 


// The buggy behavior in the RadixSort is only exposed when we use
// float arrays, but checking everything just to be sure.
for (var ctor of anyTypedArrayConstructors) {
    var testArray = new ctor(1024);
    Object.defineProperty(testArray, "buffer", { get() { throw new Error("FAIL: Buffer accessed directly"); }  });
    testArray.sort();
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
