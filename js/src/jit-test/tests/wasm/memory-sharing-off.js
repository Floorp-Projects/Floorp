// |jit-test| --shared-memory=off; skip-if: !wasmThreadsEnabled()

// A module using shared memory should be convertable from text to binary even
// if shared memory is disabled.

var bin = wasmTextToBinary('(module (memory 1 1 shared))');

// But we should not be able to validate it:

assertEq(WebAssembly.validate(bin), false);

// Nor to compile it:

assertErrorMessage(() => new WebAssembly.Module(bin),
		   WebAssembly.CompileError,
		   /shared memory is disabled/);

// We also should not be able to create a shared memory by itself:

assertErrorMessage(() => new WebAssembly.Memory({initial: 1, maximum: 1, shared: true}),
		   WebAssembly.LinkError,
		   /shared memory is disabled/);

