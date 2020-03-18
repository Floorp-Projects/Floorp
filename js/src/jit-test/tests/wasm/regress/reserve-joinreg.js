// Bug 1280933, excerpted from binary test case provided there.

wasmEvalText(
`(module
  (func $func0 (param $arg0 i32) (result i32) (local $var0 i64)
	(local.set $var0 (i64.extend_u/i32 (local.get $arg0)))
	(i32.wrap/i64
	 (i64.add
	  (block i64
	   (loop $label1 $label0
		(drop (block $label2 i64
		       (br_table $label2 (i64.const 0) (local.get $arg0))))
		(local.set $var0 (i64.mul (i64.const 2) (local.get $var0))))
	   (tee_local $var0 (i64.add (i64.const 4) (local.get $var0))))
	  (i64.const 1))))
  (export "" 0))`);
