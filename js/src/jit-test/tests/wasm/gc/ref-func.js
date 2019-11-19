// |jit-test| skip-if: !wasmReftypesEnabled()

load(libdir + "wasm-binary.js");

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

function validFuncRefText(forwardDeclare, tbl_type) {
	return wasmEvalText(`
		(module
			(table 1 ${tbl_type})
			(func $test (result funcref) ref.func $referenced)
			(func $referenced)
			${forwardDeclare}
		)
	`);
}

// referenced function must be forward declared somehow
assertErrorMessage(() => validFuncRefText('', 'funcref'), WebAssembly.CompileError, /function index is not in an element segment/);

// referenced function can be forward declared via segments
assertEq(validFuncRefText('(elem 0 (i32.const 0) func $referenced)', 'funcref') instanceof WebAssembly.Instance, true);
assertEq(validFuncRefText('(elem func $referenced)', 'funcref') instanceof WebAssembly.Instance, true);
assertEq(validFuncRefText('(elem declared $referenced)', 'funcref') instanceof WebAssembly.Instance, true);

// also when the segment is passive or active 'anyref'
assertEq(validFuncRefText('(elem 0 (i32.const 0) anyref (ref.func $referenced))', 'anyref') instanceof WebAssembly.Instance, true);
assertEq(validFuncRefText('(elem anyref (ref.func $referenced))', 'anyref') instanceof WebAssembly.Instance, true);

// referenced function cannot be forward declared via start section or export
assertErrorMessage(() => validFuncRefText('(start $referenced)', 'funcref'),
                   WebAssembly.CompileError,
                   /function index is not in an element segment/);
assertErrorMessage(() => validFuncRefText('(export "referenced" $referenced)', 'funcref'),
                   WebAssembly.CompileError,
                   /function index is not in an element segment/);

// Tests not expressible in the text format.

// element segment with elemexpr can carry non-reference type, but this must be
// rejected.

assertErrorMessage(() => new WebAssembly.Module(
    moduleWithSections([generalElemSection([{ flag: PassiveElemExpr,
                                              typeCode: I32Code,
                                              elems: [] }])])),
                   WebAssembly.CompileError,
                   /segments with element expressions can only contain references/);

// declared element segment with elemexpr can carry type anyref, but this must be rejected.

assertErrorMessage(() => new WebAssembly.Module(
    moduleWithSections([generalElemSection([{ flag: DeclaredElemExpr,
                                              typeCode: AnyrefCode,
                                              elems: [] }])])),
                   WebAssembly.CompileError,
                   /declared segment's element type must be subtype of funcref/);

// declared element segment of type funcref with elemexpr can carry a null
// value, but the null value must be rejected.

assertErrorMessage(() => new WebAssembly.Module(
    moduleWithSections([generalElemSection([{ flag: DeclaredElemExpr,
                                              typeCode: AnyFuncCode,
                                              elems: [[RefNullCode]] }])])),
                   WebAssembly.CompileError,
                   /declared element segments cannot contain ref.null/);

