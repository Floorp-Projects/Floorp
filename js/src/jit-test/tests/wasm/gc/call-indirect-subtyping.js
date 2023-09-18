// |jit-test| test-also=--wasm-tail-calls; skip-if: !wasmGcEnabled()

// Test that call_indirect will respect subtyping by defining a bunch of types
// and checking every combination of (expected, actual) type.
//
// NOTE: Several of these types are identical to each other to test
// canonicalization as well, and this causes some bloat in the 'subtypeOf'
// lists.
const TESTS = [
	{
		// (type 0) (equivalent to 1)
		type: `(type (sub (func)))`,
		subtypeOf: [0, 1],
	},
	{
		// (type 1) (equivalent to 0)
		type: `(rec (type (sub (func))))`,
		subtypeOf: [0, 1],
	},
	{
		// (type 2)
		type: `(rec (type (sub (func))) (type (sub (func))))`,
		subtypeOf: [2],
	},
	{
		// (type 3)
		// Hack entry of previous to capture that it actually defines
		// two types in the recursion group.
		type: undefined,
		subtypeOf: [3],
	},
	{
		// (type 4) (equivalent to 7)
		type: `(type (sub 0 (func)))`,
		subtypeOf: [0, 1, 4, 7],
	},
	{
		// (type 5) (equivalent to 8)
		type: `(type (sub 4 (func)))`,
		subtypeOf: [0, 1, 4, 5, 7, 8],
	},
	{
		// (type 6)
		type: `(type (sub 5 (func)))`,
		subtypeOf: [0, 1, 4, 5, 6, 7, 8],
	},
	{
		// (type 7) (equivalent to 4)
		type: `(type (sub 0 (func)))`,
		subtypeOf: [0, 1, 4, 7],
	},
	{
		// (type 8) (equivalent to 5)
		type: `(type (sub 7 (func)))`,
		subtypeOf: [0, 1, 4, 5, 7, 8],
	},
	{
		// (type 9) - a final type that has an immediate form
		type: `(type (func))`,
		subtypeOf: [9],
	}
];

// Build a module with all the types, functions with those types, and functions
// that call_indirect with those types, and a table with all the functions in
// it.
let typeSection = '';
let importedFuncs = '';
let definedFuncs = '';
let callIndirectFuncs = '';
let returnCallIndirectFuncs = '';
let i = 0;
for (let {type} of TESTS) {
	if (type) {
		typeSection += type + '\n';
	}
	importedFuncs += `(func \$import${i} (import "" "import${i}") (type ${i}))\n`;
	definedFuncs += `(func \$define${i} (export "define${i}") (type ${i}))\n`;
	callIndirectFuncs += `(func (export "call_indirect ${i}") (param i32)
		(drop (ref.cast (ref ${i}) (table.get local.get 0)))
		(call_indirect (type ${i}) local.get 0)
	)\n`;
	if (wasmTailCallsEnabled()) {
		returnCallIndirectFuncs += `(func (export "return_call_indirect ${i}") (param i32)
			(drop (ref.cast (ref ${i}) (table.get local.get 0)))
			(return_call_indirect (type ${i}) local.get 0)
		)\n`;
	}
	i++;
}
let moduleText = `(module
	${typeSection}
	${importedFuncs}
	${definedFuncs}
	${callIndirectFuncs}
	${returnCallIndirectFuncs}
	(table
		(export "table")
		funcref
		(elem ${TESTS.map((x, i) => `\$import${i} \$define${i}`).join(" ")})
	)
)`;

// Now go over every combination of (actual, expected). In this case the caller
// (which does the call_indirect) specifies expected and the callee will be the
// actual.
let imports = {
	"": Object.fromEntries(TESTS.map((x, i) => [`import${i}`, () => {}])),
};
let exports = wasmEvalText(moduleText, imports).exports;
for (let callerTypeIndex = 0; callerTypeIndex < TESTS.length; callerTypeIndex++) {
	for (let calleeTypeIndex = 0; calleeTypeIndex < TESTS.length; calleeTypeIndex++) {
		let calleeType = TESTS[calleeTypeIndex];

		// If the callee (actual) is a subtype of caller (expected), then this
		// should succeed.
		let shouldPass = calleeType.subtypeOf.includes(callerTypeIndex);

		let calleeImportFuncIndex = calleeTypeIndex * 2;
		let calleeDefinedFuncIndex = calleeTypeIndex * 2 + 1;

		// print(`expected (type ${callerTypeIndex}) (actual ${calleeTypeIndex})`);
		let test = () => {
			exports[`call_indirect ${callerTypeIndex}`](calleeImportFuncIndex)
			exports[`call_indirect ${callerTypeIndex}`](calleeDefinedFuncIndex)
			if (wasmTailCallsEnabled()) {
				exports[`return_call_indirect ${callerTypeIndex}`](calleeImportFuncIndex)
				exports[`return_call_indirect ${callerTypeIndex}`](calleeDefinedFuncIndex)
			}
		};
		if (shouldPass) {
			test();
		} else {
			assertErrorMessage(test, WebAssembly.RuntimeError, /mismatch|cast/);
		}
	}
}
