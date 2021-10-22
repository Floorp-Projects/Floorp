// Bug 1731039 fuzzbug. Exercises one of the new ArrayBufferOrView classes.

var error;
try {
  encodeAsUtf8InBuffer("", "");
} catch (e) {
  error = e;
}

assertEq(error.message.includes("must be a Uint8Array"), true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
