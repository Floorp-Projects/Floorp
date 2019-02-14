// |jit-test| skip-if: !wasmGeneralizedTables()

for ( let prefix of ['', '(table $prefix 0 32 anyfunc)']) {
    let mod = new WebAssembly.Module(wasmTextToBinary(
    `(module
       (gc_feature_opt_in 3)
       ${prefix}
       (table $tbl 0 anyref)
       (import $item "m" "item" (func (result anyref)))
       (func (export "run") (param $numiters i32)
         (local $i i32)
         (local $j i32)
         (local $last i32)
         (local $iters i32)
         (local $tmp anyref)
         (set_local $last (table.grow $tbl (i32.const 1) (ref.null)))
         (table.set $tbl (get_local $last) (call $item))
         (loop $iter_continue
           (set_local $i (i32.const 0))
           (set_local $j (get_local $last))
           (block $l_break
             (loop $l_continue
               (br_if $l_break (i32.ge_s (get_local $j) (get_local $i)))
               (set_local $tmp (table.get $tbl (get_local $i)))
               (if (i32.eqz (i32.rem_s (get_local $i) (i32.const 3)))
                   (set_local $tmp (call $item)))
               (table.set $tbl (get_local $i) (table.get $tbl (get_local $j)))
               (table.set $tbl (get_local $j) (get_local $tmp))
               (set_local $i (i32.add (get_local $i) (i32.const 1)))
               (set_local $j (i32.sub (get_local $j) (i32.const 1)))
               (br $l_continue))
           (set_local $iters (i32.add (get_local $iters) (i32.const 1)))
           (br_if $iter_continue (i32.lt_s (get_local $iters) (get_local $numiters)))))))`));

    for (let [mode,freq] of [[14,100],  // Compact every 100 allocations
                             [2,12],    // Collect every 12 allocations
                             [7,100],   // Minor gc every 100 allocations
                             [15,100]]) // Verify heap integrity
    {
        if (this.gczeal)
            this.gczeal(mode,freq);
        let k = 0;
        let ins = new WebAssembly.Instance(mod, {m:{item:() => { return { x: k++ } }}}).exports;
        for ( let i=0; i < 1000; i++ )
            ins.run(1000);
    }
}


