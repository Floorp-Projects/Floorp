// |jit-test| skip-if: !wasmReftypesEnabled()

load(libdir + "wasm-binary.js");

const v2vSig = {args:[], ret:VoidCode};
const v2vSigSection = sigSection([v2vSig]);

// 'ref.func' parses, validates and returns a non-null value
wasmFullPass(`
	(module
		(elem declare $run)
		(func $run (result i32)
			ref.func $run
			ref.is_null
		)
		(export "run" (func $run))
	)
`, 0);

// function returning reference to itself
{
	let {f1} = wasmEvalText(`
		(module
			(elem declare $f1)
			(func $f1 (result funcref) ref.func $f1)
			(export "f1" (func $f1))
		)
	`).exports;
	assertEq(f1(), f1);
}

// function returning reference to a different function
{
	let {f1, f2} = wasmEvalText(`
		(module
			(elem declare $f1)
			(func $f1)
			(func $f2 (result funcref) ref.func $f1)
			(export "f1" (func $f1))
			(export "f2" (func $f2))
		)
	`).exports;
	assertEq(f2(), f1);
}

// function returning reference to function in a different module
{
	let i1 = wasmEvalText(`
		(module
			(elem declare $f1)
			(func $f1)
			(export "f1" (func $f1))
		)
	`);
	let i2 = wasmEvalText(`
		(module
			(import "" "f1" (func $f1))
			(elem declare $f1)
			(func $f2 (result funcref) ref.func $f1)
			(export "f1" (func $f1))
			(export "f2" (func $f2))
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
assertEq(validFuncRefText('(elem declare $referenced)', 'funcref') instanceof WebAssembly.Instance, true);

// also when the segment is passive or active 'anyref'
assertEq(validFuncRefText('(elem 0 (i32.const 0) anyref (ref.func $referenced))', 'anyref') instanceof WebAssembly.Instance, true);
assertEq(validFuncRefText('(elem anyref (ref.func $referenced))', 'anyref') instanceof WebAssembly.Instance, true);

// referenced function cannot be forward declared via start section or export
assertErrorMessage(() => validFuncRefText('(start $referenced)', 'funcref'),
                   WebAssembly.CompileError,
                   /function index is not in an element segment/);
assertErrorMessage(() => validFuncRefText('(export "referenced" (func $referenced))', 'funcref'),
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

// Test case for bug 1596026: when taking the ref.func of an imported function,
// the value obtained should not be the JS function.  This would assert (even in
// a release build), so the test is merely that the code runs.

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (module
    (import "m" "f" (func $f (param i32) (result i32)))
    (elem declare $f)
    (table 1 funcref)
    (func (export "f")
      (table.set 0 (i32.const 0) (ref.func $f))))`)),
                                   {m:{f:(x) => 37+x}});
ins.exports.f();

// Verification that we can handle encoding errors for passive element segments
// properly.

function checkPassiveElemSegment(mangle, err) {
    let bin = moduleWithSections(
        [v2vSigSection, declSection([0]), // One function
         tableSection(1),                 // One table
         { name: elemId,                  // One passive segment
           body: (function () {
               let body = [];
               body.push(1);           // 1 element segment
               body.push(0x1 | 0x4);   // Flags: Passive and uses element expression
               body.push(AnyFuncCode + (mangle == "type" ? 1 : 0)); // always anyfunc
               body.push(1);           // Element count
               body.push(RefFuncCode + (mangle == "ref.func" ? 1 : 0)); // always ref.func
               body.push(0);           // func index
               body.push(EndCode + (mangle == "end" ? 1 : 0));
               return body;
           })() },
         bodySection(                   // Empty function
             [funcBody(
                 {locals:[],
                  body:[]})])
        ]);
    if (err) {
        assertErrorMessage(() => new WebAssembly.Module(bin),
                           WebAssembly.CompileError,
                           err);
    } else {
        new WebAssembly.Module(bin);
    }
}

checkPassiveElemSegment("");
checkPassiveElemSegment("type", /segments with element expressions can only contain references/);
checkPassiveElemSegment("ref.func", /failed to read initializer operation/);
checkPassiveElemSegment("end", /failed to read end of initializer expression/);

// Passive element segments can contain literal null values.

{
    let txt =
        `(module
           (table (export "t") 10 funcref)
           (elem (i32.const 1) $m)
           (elem (i32.const 3) $m)
           (elem (i32.const 6) $m)
           (elem (i32.const 8) $m)
           (elem funcref (ref.func $f) (ref.null) (ref.func $g) (ref.null) (ref.func $h))
           (func $m)
           (func $f)
           (func $g)
           (func $h)
           (func (export "doit") (param $idx i32)
             (table.init 4 (local.get $idx) (i32.const 0) (i32.const 5))))`;
    let ins = wasmEvalText(txt);
    ins.exports.doit(0);
    ins.exports.doit(5);
    assertEq(typeof ins.exports.t.get(0), "function");
    assertEq(ins.exports.t.get(1), null);
    assertEq(typeof ins.exports.t.get(2), "function");
    assertEq(ins.exports.t.get(0) == ins.exports.t.get(2), false);
    assertEq(ins.exports.t.get(3), null);
    assertEq(typeof ins.exports.t.get(4), "function");
    assertEq(typeof ins.exports.t.get(5), "function");
    assertEq(ins.exports.t.get(6), null);
    assertEq(typeof ins.exports.t.get(7), "function");
    assertEq(ins.exports.t.get(8), null);
    assertEq(typeof ins.exports.t.get(9), "function");
}

