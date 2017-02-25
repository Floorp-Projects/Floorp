wasmFullPass(`(module
    (func $test (result i32) (param i32) (param i32) (i32.add (get_local 0) (get_local 1)))
    (func $run (result i32) (call $test (i32.const 1) (i32.const ${Math.pow(2, 31) - 1})))
    (export "run" $run)
)`, -Math.pow(2, 31));

wasmFullPass(`(module
    (func (result i32)
        i32.const 1
        i32.const 42
        i32.add
        return
        unreachable
        i32.const 0
        call 3
        i32.const 42
        i32.add
    )
    (func) (func) (func)
(export "run" 0))`, 43);

// Global section.
wasmFullPass(`(module
 (import $imported "globals" "x" (global i32))
 (global $mut_local (mut i32) (i32.const 0))
 (global $imm_local i32 (i32.const 37))
 (global $imm_local_2 i32 (get_global 0))
 (func $get (result i32)
  i32.const 13
  set_global $mut_local
  get_global $imported
  get_global $mut_local
  i32.add
  get_global $imm_local
  i32.add
  get_global $imm_local_2
  i32.add
 )
 (export "run" $get)
)`, 13 + 42 + 37 + 42, { globals: {x: 42} });

// Memory.
wasmFullPass(`(module
    (memory (export "memory") 1 2)
    (data (i32.const 0) "\\00\\01\\02" "\\03\\04\\05")
    (data (i32.const 6) "\\06")
    (func (export "run") (result i32)
        i32.const 1
        i32.load offset=2
    )
)`, 0x06050403);

let memory = new WebAssembly.Memory({ initial: 1, maximum: 2 });

wasmFullPass(`(module
    (memory (import "" "memory") 1 2)
    (data (i32.const 0) "\\00\\01\\02\\03\\04\\05")
    (func (export "run") (result i32)
        i32.const 1
        i32.load offset=2
    )
    (export "mem" memory)
)`, 0x050403, {"": {memory}});

// Tables.
wasmFullPass(`(module
    (table (export "table") 3 anyfunc)
    (type $t (func (result i32)))
    (func $foo (result i32) (i32.const 1))
    (func $bar (result i32) (i32.const 2))
    (func $baz (result i32) (i32.const 3))
    (elem (i32.const 0) $baz $bar)
    (elem (i32.const 2) $foo)
    (func (export "run") (param i32) (result i32)
        get_local 0
        call_indirect $t
    )
)`, 3, {}, 0);

let table = new WebAssembly.Table({ element: 'anyfunc', initial: 3, maximum: 3 });

wasmFullPass(`(module
    (table (import "" "table") 3 4 anyfunc)
    (type $t (func (result i32)))
    (func $foo (result i32) (i32.const 1))
    (func $bar (result i32) (i32.const 2))
    (func $baz (result i32) (i32.const 3))
    (elem (i32.const 0) $baz $bar)
    (elem (i32.const 2) $foo)
    (func (export "run") (param i32) (result i32)
        get_local 0
        call_indirect $t
    )
)`, 3, {"":{table}}, 0);

// Start function.
wasmFullPass(`(module
    (global $g (mut i32) (i32.const 0))
    (func $start
        get_global $g
        i32.const 1
        i32.add
        set_global $g
    )
    (start $start)
    (func (export "run") (result i32)
        get_global $g
    )
)`, 1);

// Branch table.
for (let [p, result] of [
    [0, 7],
    [1, 6],
    [2, 4],
    [42, 4]
]) {
    wasmFullPass(`(module
        (func (export "run") (result i32) (param $p i32) (local $n i32)
            i32.const 0
            set_local $n
            block $c block $b block $a
                get_local $p
                br_table $a $b $c
            end $a
                get_local $n
                i32.const 1
                i32.add
                set_local $n
            end $b
                get_local $n
                i32.const 2
                i32.add
                set_local $n
            end $c
            get_local $n
            i32.const 4
            i32.add
        )
    )`, result, {}, p);
}
