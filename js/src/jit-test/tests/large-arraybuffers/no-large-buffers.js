// |jit-test| --no-large-arraybuffers

load(libdir + "asserts.js")

assertErrorMessage(() => new ArrayBuffer(2 * 1024 * 1024 * 1024),
                   RangeError,
                   /invalid array length/)
