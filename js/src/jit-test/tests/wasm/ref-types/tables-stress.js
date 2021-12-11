for ( let prefix of ['', '(table $prefix 0 32 funcref)']) {
    let mod = new WebAssembly.Module(wasmTextToBinary(
    `(module
       (import "m" "item" (func $item (result externref)))
       ${prefix}
       (table $tbl 0 externref)
       (func (export "run") (param $numiters i32)
         (local $i i32)
         (local $j i32)
         (local $last i32)
         (local $iters i32)
         (local $tmp externref)
         (local.set $last (table.grow $tbl (ref.null extern) (i32.const 1)))
         (table.set $tbl (local.get $last) (call $item))
         (loop $iter_continue
           (local.set $i (i32.const 0))
           (local.set $j (local.get $last))
           (block $l_break
             (loop $l_continue
               (br_if $l_break (i32.ge_s (local.get $j) (local.get $i)))
               (local.set $tmp (table.get $tbl (local.get $i)))
               (if (i32.eqz (i32.rem_s (local.get $i) (i32.const 3)))
                   (local.set $tmp (call $item)))
               (table.set $tbl (local.get $i) (table.get $tbl (local.get $j)))
               (table.set $tbl (local.get $j) (local.get $tmp))
               (local.set $i (i32.add (local.get $i) (i32.const 1)))
               (local.set $j (i32.sub (local.get $j) (i32.const 1)))
               (br $l_continue))
           (local.set $iters (i32.add (local.get $iters) (i32.const 1)))
           (br_if $iter_continue (i32.lt_s (local.get $iters) (local.get $numiters)))))))`));

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


