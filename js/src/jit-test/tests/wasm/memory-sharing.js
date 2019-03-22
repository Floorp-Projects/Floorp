// |jit-test| skip-if: !wasmThreadsSupported()

const WASMPAGE = 65536;

// A shared memory should yield a SharedArrayBuffer of appropriate length

{
    let mem = new WebAssembly.Memory({initial: 2, maximum: 4, shared: true});
    assertEq(mem.buffer instanceof SharedArrayBuffer, true);
    assertEq(mem.buffer.byteLength, WASMPAGE*2);
}

// Ditto, when the memory was created by instantiation and exported

{
    let text = `(module
		 (memory (export "memory") 1 2 shared)
		 (func (export "l0") (result i32) (i32.load (i32.const 0))))`;
    let mod = new WebAssembly.Module(wasmTextToBinary(text));
    let ins = new WebAssembly.Instance(mod);
    let mem = ins.exports.memory;
    let buf = mem.buffer;
    assertEq(buf instanceof SharedArrayBuffer, true);
    assertEq(buf.byteLength, WASMPAGE);
}

// Shared memory requires a maximum size

{
    assertErrorMessage(() => new WebAssembly.Memory({initial: 2, shared: true}),
		       TypeError,
		       /'shared' is true but maximum is not specified/);
}

// Ditto, syntactically

{
    let text = `(module
		 (memory 1 shared)
		 (func (export "l0") (result i32) (i32.load (i32.const 0))))`;
    assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(text)),
		       WebAssembly.CompileError,
		       /maximum length required for shared memory/);
}

// Ditto, in the binary.  The flags field can be 0 (unshared, min only), 1
// (unshared, min and max), or 3 (shared, min and max), but not 2 (shared, min
// only).  So construct a module that has that, and make sure it's rejected.

{
    let bin = new Uint8Array([0x00, 0x61, 0x73, 0x6d,
			      0x01, 0x00, 0x00, 0x00,
			      0x05,                   // Memory
			      0x03,                   // Section size
			      0x01,                   // One memory
			      0x02,		      // Shared, min only (illegal)
			      0x01]);		      // Min
    assertErrorMessage(() => new WebAssembly.Module(bin),
		       WebAssembly.CompileError,
		       /maximum length required for shared memory/);
}

// Importing shared memory and being provided with shared should work

{
    let text = `(module
		 (memory (import "" "memory") 1 1 shared)
		 (func (export "id") (param i32) (result i32) (local.get 0)))`;
    let mod = new WebAssembly.Module(wasmTextToBinary(text));
    let mem = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
    let ins = new WebAssembly.Instance(mod, {"": {memory: mem}});
    assertEq(ins.exports.id(0x12345678), 0x12345678);
}

// Importing shared memory but being provided with unshared should fail

{
    let text = `(module
		 (memory (import "" "memory") 1 1 shared)
		 (func (export "id") (param i32) (result i32) (local.get 0)))`;
    let mod = new WebAssembly.Module(wasmTextToBinary(text));
    let mem = new WebAssembly.Memory({initial: 1, maximum: 1});
    assertErrorMessage(() => new WebAssembly.Instance(mod, {"": {memory: mem}}),
		       WebAssembly.LinkError,
		       /unshared memory but shared required/);
}

// Importing unshared memory but being provided with shared should fail

{
    let text = `(module
		 (memory (import "" "memory") 1 1)
		 (func (export "id") (param i32) (result i32) (local.get 0)))`;
    let mod = new WebAssembly.Module(wasmTextToBinary(text));
    let mem = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
    assertErrorMessage(() => new WebAssembly.Instance(mod, {"": {memory: mem}}),
		       WebAssembly.LinkError,
		       /shared memory but unshared required/);
}

// Importing shared memory and being provided with shared memory with
// incompatible parameters should fail

{
    let text = `(module
		 (memory (import "" "memory") 2 4 shared)
		 (func (export "id") (param i32) (result i32) (local.get 0)))`;
    let mod = new WebAssembly.Module(wasmTextToBinary(text));

    // some cases that are non-matching are allowed, eg, initial > declared min

    // initial < declared min
    let mem3 = new WebAssembly.Memory({initial: 1, maximum: 4, shared: true});
    assertErrorMessage(() => new WebAssembly.Instance(mod, {"": {memory: mem3}}),
		       WebAssembly.LinkError,
		       /imported Memory with incompatible size/);

    // initial > declared max
    let mem4 = new WebAssembly.Memory({initial: 5, maximum: 8, shared: true});
    assertErrorMessage(() => new WebAssembly.Instance(mod, {"": {memory: mem4}}),
		       WebAssembly.LinkError,
		       /imported Memory with incompatible size/);

    // max > declared max
    let mem5 = new WebAssembly.Memory({initial: 2, maximum: 8, shared: true});
    assertErrorMessage(() => new WebAssembly.Instance(mod, {"": {memory: mem5}}),
		       WebAssembly.LinkError,
		       /imported Memory with incompatible maximum size/);
}


// basic memory.size and memory.grow operation, with bounds checking near the
// valid/invalid boundary

{
    let text = `(module
		 (memory (export "memory") 2 4 shared)
		 (func (export "c") (result i32) memory.size)
		 (func (export "g") (result i32) (memory.grow (i32.const 1)))
		 (func (export "l") (param i32) (result i32) (i32.load (local.get 0)))
		 (func (export "s") (param i32) (param i32) (i32.store (local.get 0) (local.get 1))))`;
    let mod = new WebAssembly.Module(wasmTextToBinary(text));
    let ins = new WebAssembly.Instance(mod);
    let exp = ins.exports;
    let mem = exp.memory;

    let b1 = mem.buffer;
    assertEq(exp.c(), 2);
    assertEq(b1.byteLength, WASMPAGE*2);
    assertEq(mem.buffer === b1, true);   // memory.size does not affect buffer
    exp.s(WASMPAGE*2-4, 0x12345678)	 // access near end
    assertEq(exp.l(WASMPAGE*2-4), 0x12345678);
    assertErrorMessage(() => exp.l(WASMPAGE*2), // beyond current end (but below max)
		       WebAssembly.RuntimeError,
		       /index out of bounds/);
    assertEq(exp.g(), 2);
    assertEq(b1.byteLength, WASMPAGE*2); // growing does not affect existing buffer length
    let b2 = mem.buffer;
    assertEq(b1 !== b2, true);	         // growing produces a new buffer
    assertEq(b2.byteLength, WASMPAGE*3); // new buffer has appropriate length
    assertEq(exp.c(), 3);
    exp.s(WASMPAGE*3-4, 0x12344321);     // access near new end
    assertEq(exp.l(WASMPAGE*3-4), 0x12344321);
    assertErrorMessage(() => exp.l(WASMPAGE*3), // beyond current end (but below max)
		       WebAssembly.RuntimeError,
		       /index out of bounds/);
    assertEq(exp.g(), 3);
    assertEq(b2.byteLength, WASMPAGE*3); // growing does not affect existing buffer length
    let b3 = mem.buffer;
    assertEq(b2 !== b3, true);	         // growing produces a new buffer
    assertEq(b3.byteLength, WASMPAGE*4); // new buffer has appropriate length
    assertEq(exp.c(), 4);
    exp.s(WASMPAGE*4-4, 0x12121212);     // access near new end
    assertEq(exp.l(WASMPAGE*4-4), 0x12121212);
    assertErrorMessage(() => exp.l(WASMPAGE*4), // beyond current end (and beyond max)
		       WebAssembly.RuntimeError,
		       /index out of bounds/);
    assertEq(exp.g(), -1);
    assertEq(exp.c(), 4);                // failure to grow -> no change
    let b4 = mem.buffer;
    assertEq(b3 === b4, true);	         // failure to grow -> same ol' buffer
    assertEq(exp.g(), -1);               // we can fail repeatedly
}

// Test the grow() API with shared memory.  In the implementation this API
// shares almost all code with the wasm instruction, so don't bother going deep.

{
    let mem = new WebAssembly.Memory({initial: 2, maximum: 4, shared: true});
    let buf = mem.buffer;
    assertEq(mem.grow(1), 2);
    assertEq(buf.byteLength, WASMPAGE*2);
    assertEq(mem.grow(1), 3);
    assertErrorMessage(() => mem.grow(1), RangeError, /failed to grow memory/);
}

// Initializing shared memory with data

{
    let text = `(module
		 (memory (import "" "memory") 2 4 shared)
		 (data (i32.const 0) "abcdefghijklmnopqrstuvwxyz")
		 (func (export "l") (param i32) (result i32) (i32.load8_u (local.get 0))))`;
    let mod = new WebAssembly.Module(wasmTextToBinary(text));
    let mem = new WebAssembly.Memory({initial: 2, maximum: 4, shared: true});
    let ins = new WebAssembly.Instance(mod, {"": {memory: mem}});
    let exp = ins.exports;
    assertEq(exp.l(12), "a".charCodeAt(0) + 12);
}

