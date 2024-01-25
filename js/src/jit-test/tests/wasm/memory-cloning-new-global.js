// |jit-test| skip-if: !wasmThreadsEnabled()

// Ensure we create the WebAssembly.Memory prototype object when deserializing a
// WebAssembly.Memory object in a new global.

let mem1 = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
let buf = serialize(mem1, [], {SharedArrayBuffer: "allow"});

let g = newGlobal({sameCompartmentAs: this});
let mem2 = g.deserialize(buf, {SharedArrayBuffer: "allow"});
assertEq(mem2.buffer.byteLength, 65536);
assertEq(Object.getPrototypeOf(mem2), g.WebAssembly.Memory.prototype);
