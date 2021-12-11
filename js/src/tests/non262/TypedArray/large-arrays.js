// |reftest| skip-if(!xulRuntime.shell)

// Test that we can access TypedArrays beyond the 4GB mark, if large buffers are
// supported.

const gb = 1024 * 1024 * 1024;

if (largeArrayBufferEnabled()) {
    for (let TA of typedArrayConstructors) {
        let ta = new TA(6*gb / TA.BYTES_PER_ELEMENT);

        // Set element at the 5GB mark
        ta[5*gb / TA.BYTES_PER_ELEMENT] = 37;

        // Check that it was set
        assertEq(ta[5*gb / TA.BYTES_PER_ELEMENT], 37);

        // Check that we're not operating mod 4GB
        assertEq(ta[1*gb / TA.BYTES_PER_ELEMENT], 0);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
