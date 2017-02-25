// Test bounds checking at the end of memory with a constant base pointer.
// This is intended to verify a bounds check elimination optimization in
// the baseline compiler.

// This can be only a functional test, we can't really see whether
// the optimization is being applied.  However, manual inspection
// of the generated code has verified that the optimization is
// being applied.

// Memory is one page and minimum memory is one page, so accesses across
// the end of the first page should fail.

function gen(base, offset) {
  return wasmEvalText(`(module
			(memory 1)
			(data (i32.const 65528) "aaaaaaaa")
			(func (result i32)
			 (i32.load offset=${offset} (i32.const ${base})))
			(export "f" 0))`).exports["f"];
}

// Memory is two pages but minimum memory is one page, so accesses across
// the end of the first page should succeed.

function gen2(base, offset) {
  return wasmEvalText(`(module
			(memory 1)
			(data (i32.const 65528) "aaaaaaaa")
			(func (result i32)
			 (drop (grow_memory (i32.const 1)))
			 (i32.store (i32.const 65536) (i32.const 0x61616161))
			 (i32.store (i32.const 65540) (i32.const 0x61616161))
			 (i32.store (i32.const 80000) (i32.const 0x61616161))
			 (i32.store (i32.const 120000) (i32.const 0x61616161))
			 (i32.load offset=${offset} (i32.const ${base})))
			(export "f" 0))`).exports["f"];
}

// Access the first page.
// None of these should have an explicit bounds check generated for them as the
// effective address is less than the sum of the minimum heap length and the
// offset guard length.

let onfirst = [[4,65528], [3,65528], [2,65528], [1,65528], [0,65528],
	       [3,65529], [2,65529], [1,65529], [0,65529],
	       [2,65530], [1,65530], [0,65530],
	       [1,65531], [0,65531],
	       [0,65532],
	       [256, 65536-256-4],
	       [2048, 65536-2048-4],
	       [30000, 65536-30000-4]];

for ( let [x,y] of onfirst ) {
  assertEq(gen(x,y)(), 0x61616161|0);
  assertEq(gen(y,x)(), 0x61616161|0);
}

// Access the second page.
// Again, no explicit bounds checks should be generated here.

let onsecond = [[65532,1], [65532,2], [65532,3], [65532,4], [65532,5], [65532,6], [65532,7], [65532,8],
		[65531,2], [65531,3], [65531,4], [65531,5], [65531,6], [65531,7], [65531,8], [65531,9],
		[65530,3], [65530,4], [65530,5], [65530,6], [65530,7], [65530,8], [65530,9], [65530,10],
		[65529,4], [65529,5], [65529,6], [65529,7], [65529,8], [65529,9], [65529,10], [65529,11],
		[65528,5], [65528,6], [65528,7], [65528,8], [65528,9], [65528,10], [65528,11], [65528,12],
		[65527,6], [65527,7], [65527,8], [65527,9], [65527,10], [65527,11], [65527,12], [65527,13],
		[65526,7], [65526,8], [65526,9], [65526,10], [65526,11], [65526,12], [65526,13], [65526,14],
		[65525,8], [65525,9], [65525,10], [65525,11], [65525,12], [65525,13], [65525,14], [65525,15],
		[256, 65536-256],
		[2048, 65536-2048],
		[30000, 50000],
		[30000, 90000]];

for ( let [x,y] of onsecond ) {
  assertErrorMessage(() => { gen(x,y)() }, WebAssembly.RuntimeError, /index out of bounds/);
  assertErrorMessage(() => { gen(y,x)() }, WebAssembly.RuntimeError, /index out of bounds/);

  assertEq(gen2(x,y)(), 0x61616161|0);
  assertEq(gen2(y,x)(), 0x61616161|0);
}

// Access the third page.
// Here the explicit bounds check cannot be omitted, as the access is
// beyond min + offset guard.

let onthird = [[60000, 90000]];

for ( let [x,y] of onthird ) {
  assertErrorMessage(() => { gen(x,y)() }, WebAssembly.RuntimeError, /index out of bounds/);
  assertErrorMessage(() => { gen(y,x)() }, WebAssembly.RuntimeError, /index out of bounds/);

  assertErrorMessage(() => { gen2(x,y)() }, WebAssembly.RuntimeError, /index out of bounds/);
  assertErrorMessage(() => { gen2(y,x)() }, WebAssembly.RuntimeError, /index out of bounds/);
}
