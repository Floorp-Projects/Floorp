// |jit-test| skip-if: !wasmGcEnabled()

// Output type is non-nullable if the input type is non-nullable
wasmValidateText(`(module
	(func (param externref) (result anyref)
		local.get 0
		any.convert_extern
	)
	(func (param anyref) (result externref)
		local.get 0
		extern.convert_any
	)
	(func (param (ref extern)) (result (ref any))
		local.get 0
		any.convert_extern
	)
	(func (param (ref any)) (result (ref extern))
		local.get 0
		extern.convert_any
	)
	(func (result (ref any))
	    unreachable
	    any.convert_extern
	)
	(func (result (ref extern))
	    unreachable
	    extern.convert_any
	)
)`);

// Output type is nullable if the input type is nullable
wasmFailValidateText(`(module
	(func (param externref) (result (ref any))
		local.get 0
		any.convert_extern
	)
)`, /expected/);
wasmFailValidateText(`(module
	(func (param anyref) (result (ref extern))
		local.get 0
		extern.convert_any
	)
)`, /expected/);

// Can round trip an externref through anyref and get the same thing back
let {roundtripThroughAny} = wasmEvalText(`(module
	(func (export "roundtripThroughAny") (param externref) (result externref)
	  local.get 0
	  any.convert_extern
	  extern.convert_any
	)
)`).exports;
for (let value of WasmExternrefValues) {
	assertEq(value, roundtripThroughAny(value));
}

// Can round trip GC objects through externref and get the same thing back
let {testStruct, testArray} = wasmEvalText(`(module
	(type $struct (struct))
	(type $array (array i32))

	(func (export "testStruct") (result i32)
	  (local (ref $struct))
	  (local.set 0 struct.new $struct)
      local.get 0
	  (ref.eq
	  	(ref.cast (ref null $struct)
	  	  (any.convert_extern
	  	    (extern.convert_any
	  	      local.get 0
	  	    )
	  	  )
	  	)
	  )
	)

	(func (export "testArray") (result i32)
	  (local (ref $array))
	  (local.set 0 (array.new $array i32.const 0 i32.const 0))
      local.get 0
	  (ref.eq
	  	(ref.cast (ref null $array)
	  	  (any.convert_extern
	  	    (extern.convert_any
	  	      local.get 0
	  	    )
	  	  )
	  	)
	  )
	)
)`).exports;

assertEq(testStruct(), 1);
assertEq(testArray(), 1);
