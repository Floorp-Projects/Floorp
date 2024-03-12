// |reftest| skip-if(!xulRuntime.shell)

// Compressed buffers must have magic header and length
assertThrows(() => decompressLZ4(new ArrayBuffer()));

// Compress and decompress take an array buffer, not arrays
assertThrows(() => compressLZ4([]));
assertThrows(() => decompressLZ4([]));

// Round trip several buffers
let tests = [
	new Uint8Array([]),
	new Uint8Array([0]),
	new Uint8Array([0, 1, 2, 3]),
	new Uint8Array(1000),
];

for (let test of tests) {
	let original = test.buffer;

	let compressed = compressLZ4(original);
	assertEq(compressed instanceof ArrayBuffer, true);

	let decompressed = decompressLZ4(compressed);
	assertEq(decompressed instanceof ArrayBuffer, true);

	assertEqArray(new Uint8Array(original), new Uint8Array(decompressed));
}

reportCompare(true,true);
