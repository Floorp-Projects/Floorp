const MemoryMaxValid = 65536;

// Linking should fail if the imported memory has a higher maximum than required,
// however if we internally clamp maximum values to an implementation limit
// and use that for linking we may erroneously accept some modules.

function testLinkFail(importMax, importedMax) {
  assertErrorMessage(() => {
    let importedMemory = new WebAssembly.Memory({
      initial: 0,
      maximum: importedMax,
    });
    wasmEvalText(`(module
      (memory (import "" "") 0 ${importMax})
    )`, {"": {"": importedMemory}});
  }, WebAssembly.LinkError, /incompatible maximum/);
}

testLinkFail(0, 1);
testLinkFail(MemoryMaxValid - 1, MemoryMaxValid);

// The type reflection interface for WebAssembly.Memory should not report
// an internally clamped maximum.

if ('type' in WebAssembly.Memory.prototype) {
  let memory = new WebAssembly.Memory({
    initial: 0,
    maximum: MemoryMaxValid,
  });
  let type = memory.type();
  assertEq(type.maximum, MemoryMaxValid, 'reported memory maximum is not clamped');
}
