// An export name may contain a null terminator
let exports = wasmEvalText(`(module
	(func)
	(export "\\00first" (func 0))
	(export "\\00second" (func 0))
)`).exports;
assertEq(exports["\0first"] instanceof Function, true);
assertEq(exports["\0second"] instanceof Function, true);

// An import name may contain a null terminator
let imports = {
	"\0module": {
		"\0field": 10,
	}
};
let {global} = wasmEvalText(`(module
	(import "\\00module" "\\00field" (global i32))
	(export "global" (global 0))
)`, imports).exports;
assertEq(global.value, 10);
