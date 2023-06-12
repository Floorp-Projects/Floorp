// |jit-test| skip-if: !wasmGcEnabled()

// We can read the object fields from JS via a builtin
{
    let ins = wasmEvalText(`(module
                             (type $p (struct (field f64) (field (mut i32))))

                             (func (export "mkp") (result eqref)
                              (struct.new $p (f64.const 1.5) (i32.const 33))))`).exports;

    let p = ins.mkp();
    assertEq(wasmGcReadField(p, 0), 1.5);
    assertEq(wasmGcReadField(p, 1), 33);
    assertErrorMessage(() => wasmGcReadField(p, 2), WebAssembly.RuntimeError, /index out of bounds/);
}

// Fields can't be modified from JS.
{
    let ins = wasmEvalText(`(module
                             (type $p (struct (field f64)))

                             (func (export "mkp") (result eqref)
                              (struct.new $p (f64.const 1.5))))`).exports;

    let p = ins.mkp();
    assertErrorMessage(() => p[0] = 5.7, TypeError, /can't modify/);
}

// MVA v1 restriction: structs have no prototype
{
    let ins = wasmEvalText(`(module
                             (type $q (struct (field (mut f64))))
                             (type $p (struct (field (mut (ref null $q)))))

                             (type $r (struct (field (mut eqref))))

                             (func (export "mkp") (result eqref)
                              (struct.new $p (ref.null $q)))

                             (func (export "mkr") (result eqref)
                              (struct.new $r (ref.null eq))))`).exports;

    assertEq(Object.getPrototypeOf(ins.mkp()), null);
    assertEq(Object.getPrototypeOf(ins.mkr()), null);
}

// MVA v1 restriction: all fields are immutable
{
    let ins = wasmEvalText(`(module
                             (type $q (struct (field (mut f64))))
                             (type $p (struct (field (mut (ref null $q))) (field (mut eqref))))

                             (func (export "mkq") (result eqref)
                              (struct.new $q (f64.const 1.5)))

                             (func (export "mkp") (result eqref)
                              (struct.new $p (ref.null $q) (ref.null eq))))`).exports;
    let q = ins.mkq();
    assertEq(typeof q, "object");
    assertEq(wasmGcReadField(q, 0), 1.5);

    let p = ins.mkp();
    assertEq(typeof p, "object");
    assertEq(wasmGcReadField(p, 0), null);

    assertErrorMessage(() => { p[0] = q }, TypeError, /can't modify/);
    assertErrorMessage(() => { p[1] = q }, TypeError, /can't modify/);
}

// MVA v1 restriction: structs that expose i64 fields make those fields
// immutable from JS, and the structs are not constructible from JS.
{
    let ins = wasmEvalText(`(module
                             (type $p (struct (field (mut i64))))
                             (func (export "mkp") (result eqref)
                              (struct.new $p (i64.const 0x1234567887654321))))`).exports;

    let p = ins.mkp();
    assertEq(typeof p, "object");
    assertEq(wasmGcReadField(p, 0), 0x1234567887654321n)

    assertErrorMessage(() => { p[0] = 0 }, TypeError, /can't modify/);
}

// A consequence of the current mapping of i64 as two i32 fields is that we run
// a risk of struct.narrow not recognizing the difference.  So check this.
{
    let ins = wasmEvalText(
        `(module
          (type $p (struct (field i64)))
          (type $q (struct (field i32) (field i32)))
          (func $f (param eqref) (result i32)
           local.get 0
           ref.test (ref $q)
          )
          (func $g (param eqref) (result i32)
           local.get 0
           ref.test (ref $p)
          )
          (func (export "t1") (result i32)
           (call $f (struct.new $p (i64.const 0))))
          (func (export "t2") (result i32)
           (call $g (struct.new $q (i32.const 0) (i32.const 0)))))`).exports;
    assertEq(ins.t1(), 0);
    assertEq(ins.t2(), 0);
}
