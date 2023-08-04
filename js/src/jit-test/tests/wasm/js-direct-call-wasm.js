// |jit-test| --fast-warmup; --ion-offthread-compile=off

// Test that JS frames can trace wasm anyref values when they are used for
// direct calls.
{
	let {wasmFunc} = wasmEvalText(`(module
		(func (import "" "gc"))
		(func (export "wasmFunc") (param externref)
			call 0
		)
	)`, {"": {gc}}).exports;

	function jsFunc(i) {
		// Call the function twice so that the conversion to externref will be
		// GVN'ed and spilled across one of the calls
		wasmFunc(i);
		wasmFunc(i);
	}

	for (let i = 0; i < 100; i++) {
		jsFunc(i);
	}
}
