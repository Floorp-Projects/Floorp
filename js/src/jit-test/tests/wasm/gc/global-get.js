// |jit-test| skip-if: !wasmGcEnabled()

// global.get cannot refer to self or after globals after self

assertErrorMessage(() => wasmEvalText(`(module
	(global i32 global.get 0)
)`), WebAssembly.CompileError, /global/);

assertErrorMessage(() => wasmEvalText(`(module
	(global i32 global.get 1)
	(global i32 i32.const 0)
)`), WebAssembly.CompileError, /global/);

// global.get works on previous globals

{
	let {func, b, c, e} = wasmEvalText(`(module
		(func $func (export "func"))

		(global $a i32 i32.const 1)
		(global $b (export "b") i32 global.get $a)
		(global $c (export "c") i32 global.get $b)

		(global $d funcref ref.func $func)
		(global $e (export "e") funcref global.get $d)
	)`).exports;
	assertEq(b.value, 1);
	assertEq(c.value, 1);
	assertEq(e.value, func);
}
