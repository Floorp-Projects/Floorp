// |jit-test| skip-if: !wasmThreadsSupported()

// Basic structured cloning tests (specific to SpiderMonkey shell)

// Should *not* be possible to serialize and deserialize memories that are not
// shared, whether we transfer them or not.

{
    let mem1 = new WebAssembly.Memory({initial: 2, maximum: 4});
    assertErrorMessage(() => serialize(mem1),
		       TypeError,
		       /unsupported type for structured data/);
    assertErrorMessage(() => serialize(mem1, [mem1]),
		       TypeError,
		       /invalid transferable array for structured clone/);
}

// Should be possible to serialize and deserialize memories that are shared, and
// observe shared effects.

{
    let mem1 = new WebAssembly.Memory({initial: 2, maximum: 4, shared: true});
    let buf1 = mem1.buffer;

    // Serialization and deserialization of shared memories work:

    let mem2 = deserialize(serialize(mem1, [], {SharedArrayBuffer: 'allow'}));
    assertEq(mem2 instanceof WebAssembly.Memory, true);
    let buf2 = mem2.buffer;
    assertEq(buf2 instanceof SharedArrayBuffer, true);

    assertEq(buf1 !== buf2, true);
    assertEq(buf1.byteLength, buf2.byteLength);

    // Effects to one buffer must be reflected in the other:

    let v1 = new Int32Array(buf1);
    let v2 = new Int32Array(buf2);

    v1[37] = 0x12345678;
    assertEq(v2[37], 0x12345678);

    // Growth in a memory is reflected in its clone:

    let index = 2*65536 + 200;
    let access = wasmEvalText(`(module
				(memory (import "" "memory") 2 4 shared)
				(func (export "l") (result i32)
				 (i32.load (i32.const ${index}))))`,
			      {"": {memory: mem2}}).exports.l;

    // initially mem2 cannot be accessed at index
    assertErrorMessage(access, WebAssembly.RuntimeError, /out of bounds/);

    // then we grow mem1
    wasmEvalText(`(module
		   (memory (import "" "memory") 2 4 shared)
		   (func (export "g") (drop (memory.grow (i32.const 1)))))`,
		 {"": {memory: mem1}}).exports.g();

    // after growing mem1, mem2 can be accessed at index
    assertEq(access(), 0);
}

// Should not be possible to transfer a shared memory

{
    let mem1 = new WebAssembly.Memory({initial: 2, maximum: 4, shared: true});
    assertErrorMessage(() => serialize(mem1, [mem1]),
		       TypeError,
		       /Shared memory objects must not be in the transfer list/);

}

// When serializing and deserializing a SAB extracted from a memory, the length
// of the SAB should not change even if the memory was grown after serialization
// and before deserialization.

{
    let mem = new WebAssembly.Memory({initial: 2, maximum: 4, shared: true});
    let buf = mem.buffer;
    let clonedbuf = serialize(buf, [], {SharedArrayBuffer: 'allow'});
    mem.grow(1);
    let buf2 = deserialize(clonedbuf);
    assertEq(buf.byteLength, buf2.byteLength);
}
