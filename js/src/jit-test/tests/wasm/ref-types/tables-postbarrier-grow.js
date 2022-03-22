// Add test for issue with a post-write barrier that doesn't remove
// store buffer entries when used on a table that may grow.

let {set, table} = wasmEvalText(`(module
	(table (export "table") 1 externref)
	(func (export "set") (param externref)
		i32.const 0
		local.get 0
		table.set
	)
)`).exports;

let tenured = {};
gc(tenured);
assertEq(isNurseryAllocated(tenured), false);
let nursery = {};
assertEq(isNurseryAllocated(nursery), true);
set(nursery);
set(null);
assertEq(table.grow(1000), 1, 'table grows');
gc();
