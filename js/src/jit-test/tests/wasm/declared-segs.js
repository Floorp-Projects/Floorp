// Declared segments parse and validate
wasmFullPass(`
	(module
		(func $f1)
		(elem declare $f1)
		(elem declare funcref (ref.null func))
		(func $run)
		(export "run" (func $run))
	)
`);

// Declared segments can be used with externref
wasmFullPass(`
	(module
		(elem declare externref (ref.null extern))
		(func $run)
		(export "run" (func $run))
	)
`);

// Declared segments can be used by bulk-memory operations
wasmEvalText(`
	(module
		(func $f1)
		(table 1 1 funcref)
		(elem declare $f1)
		(func $start (table.init 0 (i32.const 0) (i32.const 0) (i32.const 1)))
		(start $start)
	)
`);

// Declared segments don't cause initialization of a table
wasmAssert(`
	(module
		(func $f1)
		(table 1 1 funcref)
		(elem declare $f1)
		(func $at (param i32) (result i32)
			local.get 0
			table.get 0
			ref.is_null
		)
		(export "at" (func $at))
	)
`, [{type: 'i32', func: '$at', args: ['i32.const 0'], expected: '1'}]);
