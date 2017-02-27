// Bug 1316181

// There are locals with different values here to ensure that the
// get_local at the end picks up the right one even if the stack might
// have become unbalanced by a failure to adjust SP along the branch
// edge.  The logic is that we use SP-relative addressing, and if the
// actual SP is not what the compiler thinks it is we will read
// something other than the expected value.

var o = wasmEvalText(
    `(module
      (func (result i32)
       (local $v0 i32)
       (local $v1 i32)
       (local $v2 i32)
       (local $v3 i32)
       (local $v4 i32)
       (local $v5 i32)
       (local $v6 i32)
       (local $v7 i32)
       (local $res i32)
       (set_local $v0 (i32.const 0xDEADBEEF))
       (set_local $v1 (i32.const 0xFDEADBEE))
       (set_local $v2 (i32.const 0xEFDEADBE))
       (set_local $v3 (i32.const 0xEEFDEADB))
       (set_local $v4 (i32.const 0xBEEFDEAD))
       (set_local $v5 (i32.const 0xDBEEFDEA))
       (set_local $v6 (i32.const 0xADBEEFDE))
       (set_local $v7 (i32.const 0xEADBEEFD))
       (block $b
	(set_local $res
	 (i32.add
	  (i32.add (i32.const 1) (i32.const 2))
	  (i32.add
	   (i32.add (i32.const 3) (i32.const 4))
	   (i32.add
	    (i32.add (i32.const 5) (i32.const 6))
	    (i32.add
	     (i32.add (i32.const 7) (i32.const 8))
	     (i32.add
	      (i32.add (i32.const 9) (i32.const 10))
	      (i32.add
	       (i32.add (i32.const 11) (i32.const 12))
	       (i32.add
		(i32.add (i32.const 13) (i32.const 14))
		(i32.add
		 (i32.add (i32.const 15) (i32.const 16))
		 (i32.add
		  (i32.add (i32.const 17) (i32.const 18))
		  (i32.add
		   (i32.add (i32.const 19) (i32.const 20))
		   (i32.add
		    (i32.add (i32.const 21) (i32.const 22))
		    (i32.add
		     (i32.add (i32.const 23) (i32.const 24))
		     (i32.add
		      (i32.add (i32.const 25) (i32.const 26))
		      (i32.add
		       (i32.add (i32.const 27) (i32.const 28))
		       (i32.add
			(i32.add (i32.const 29) (i32.const 30))
			(br_if $b (i32.const 31) (i32.const 1)))))))))))))))))))
       (return (get_local $v3)))
      (export "a" 0))`).exports;

assertEq(o["a"](), 0xEEFDEADB|0);
