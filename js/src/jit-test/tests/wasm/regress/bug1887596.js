const t = `
    (module
        (func $f (result f32)
            f32.const 1.25
        )
        (table (export "table") 10 funcref)
        (elem (i32.const 0) $f)
    )`;
const i = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(t)));
const f = i.exports.table.get(0);

// These FP equality comparisons are safe because 1.25 is representable exactly.
assertEq(1.25, f());
assertEq(1.25, this.wasmLosslessInvoke(f).value);
