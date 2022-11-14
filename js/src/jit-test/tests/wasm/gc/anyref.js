// |jit-test| skip-if: !wasmGcEnabled()

// Functions with anyref in results or params cannot be called
assertErrorMessage(() => wasmEvalText(`(module
	(func (export "func") (param anyref))
)`).exports.func(null), TypeError, /cannot pass/);
assertErrorMessage(() => wasmEvalText(`(module
	(func (export "func") (result anyref) ref.null any)
)`).exports.func(), TypeError, /cannot pass/);

// Globals of anyref cannot be accessed
let {global} = wasmEvalText(`(module
	(global (export "global") (mut anyref) ref.null any)
)`).exports;
assertErrorMessage(() => global.value, TypeError, /cannot pass/);
assertErrorMessage(() => global.value = null, TypeError, /cannot pass/);

// Tables of anyref cannot be accessed
let {table} = wasmEvalText(`(module
	(table (export "table") anyref (elem (ref.null any)))
)`).exports;
assertErrorMessage(() => table.get(0), TypeError, /cannot pass/);
assertErrorMessage(() => table.set(0, null), TypeError, /cannot pass/);
