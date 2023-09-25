// |jit-test| allow-oom; --enable-arraybuffer-transfer; skip-if: (!ArrayBuffer.prototype.transfer || getBuildConfiguration("tsan"))

let buffer = new ArrayBuffer(0);

let result = null;
try {
  result = buffer.transfer(2 ** 32);
} catch {
  // Intentionally ignore allocation failure.
}

// If the allocation succeeded, |result| has the correct size.
if (result) {
  assertEq(result.byteLength, 2 ** 32);
}
