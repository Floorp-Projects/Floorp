// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

// Bug 1280933, excerpted from binary test case provided there.

wasmEvalText(
`(module
  (func $func0 (param $var0 i32) (result i32)
	(i32.add
	 (block
	  (loop $label1 $label0
		(block $label2
		       (br_table $label0 $label1 $label2 (i64.const 0) (get_local $var0)))
		(set_local $var0 (i32.mul (i32.const 2) (get_local $var0))))
	  (set_local $var0 (i32.add (i32.const 4) (get_local $var0))))
	 (i32.const 1)))
  (export "" 0))`);
