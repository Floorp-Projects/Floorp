// |jit-test| skip-if: !wasmReftypesEnabled()

// 'ref.func' parses, validates and returns a non-null value
wasmFullPass(`
	(module
		(elem declared $run)
		(func $run (result i32)
			ref.func $run
			ref.is_null
		)
		(export "run" $run)
	)
`, 0);

// function returning reference to itself
{
	let {f1} = wasmEvalText(`
		(module
			(elem declared $f1)
			(func $f1 (result funcref) ref.func $f1)
			(export "f1" $f1)
		)
	`).exports;
	assertEq(f1(), f1);
}

// function returning reference to a different function
{
	let {f1, f2} = wasmEvalText(`
		(module
			(elem declared $f1)
			(func $f1)
			(func $f2 (result funcref) ref.func $f1)
			(export "f1" $f1)
			(export "f2" $f2)
		)
	`).exports;
	assertEq(f2(), f1);
}

// function returning reference to function in a different module
{
	let i1 = wasmEvalText(`
		(module
			(elem declared $f1)
			(func $f1)
			(export "f1" $f1)
		)
	`);
	let i2 = wasmEvalText(`
		(module
			(import $f1 "" "f1" (func))
			(elem declared $f1)
			(func $f2 (result funcref) ref.func $f1)
			(export "f1" $f1)
			(export "f2" $f2)
		)
	`, {"": i1.exports});

	let f1 = i1.exports.f1;
	let f2 = i2.exports.f2;
	assertEq(f2(), f1);
}

// function index must be valid
assertErrorMessage(() => {
	wasmEvalText(`
		(module
			(func (result funcref) ref.func 10)
		)
	`);
}, WebAssembly.CompileError, /function index out of range/);

function validFuncRefText(forwardDeclare) {
	return wasmEvalText(`
		(module
			(table 1 funcref)
			(func $test (result funcref) ref.func $referenced)
			(func $referenced)
			${forwardDeclare}
		)
	`);
}

// referenced function must be forward declared somehow
assertErrorMessage(() => validFuncRefText(''), WebAssembly.CompileError, /function index is not in an element segment/);

// referenced function can be forward declared via segments
assertEq(validFuncRefText('(elem 0 (i32.const 0) $referenced)') instanceof WebAssembly.Instance, true);
assertEq(validFuncRefText('(elem passive $referenced)') instanceof WebAssembly.Instance, true);
assertEq(validFuncRefText('(elem declared $referenced)') instanceof WebAssembly.Instance, true);

// referenced function cannot be forward declared via start section or export
assertErrorMessage(() => validFuncRefText('(start $referenced)'), WebAssembly.CompileError, /function index is not in an element segment/);
assertErrorMessage(() => validFuncRefText('(export "referenced" $referenced)'), WebAssembly.CompileError, /function index is not in an element segment/);
