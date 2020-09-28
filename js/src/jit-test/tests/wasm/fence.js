// |jit-test| skip-if: !wasmThreadsEnabled()

// Test that `atomic.fence` is a valid instruction of type `[] -> []`
wasmFullPass(
`(module
	(func atomic.fence)
	(export "run" (func 0))
)`);

// Test that `atomic.fence` works with non-shared memory
wasmFullPass(
`(module
	(memory 1)
	(func atomic.fence)
	(export "run" (func 0))
)`);

// Test that `atomic.fence` works with shared memory
wasmFullPass(
`(module
	(memory 1 1 shared)
	(func atomic.fence)
	(export "run" (func 0))
)`);
