load(libdir + "wasm.js");

assertEq(wasmEvalText(`
(module
  (func $f (param $p i32)
    block $out
      i32.const 0
      if
        i32.const 1
        tee_local $p
        br_table $out $out
      end
    end
    get_local $p
    br_if 0
  )
  (export "f" $f)
)
`).exports.f(42), undefined);
