// |jit-test| skip-if: !wasmGcEnabled() || wasmCompileMode() != 'baseline'

// We can read the object fields from JS, and write them if they are mutable.

{
    let ins = wasmEvalText(`(module
                             (type $p (struct (field f64) (field (mut i32))))

                             (func (export "mkp") (result anyref)
                              (struct.new $p (f64.const 1.5) (i32.const 33))))`).exports;

    let p = ins.mkp();
    assertEq(p._0, 1.5);
    assertEq(p._1, 33);
    assertEq(p._2, undefined);

    p._1 = 44;
    assertEq(p._1, 44);
}

// Writing an immutable field from JS throws.

{
    let ins = wasmEvalText(`(module
                             (type $p (struct (field f64)))

                             (func (export "mkp") (result anyref)
                              (struct.new $p (f64.const 1.5))))`).exports;

    let p = ins.mkp();
    assertErrorMessage(() => p._0 = 5.7,
                       Error,
                       /setting immutable field/);
}

// MVA v1 restriction: structs that expose ref-typed fields should not be
// constructible from JS.
//
// However, if the fields are anyref the structs can be constructed from JS.

{
    let ins = wasmEvalText(`(module
                             (type $q (struct (field (mut f64))))
                             (type $p (struct (field (mut (ref opt $q)))))

                             (type $r (struct (field (mut anyref))))

                             (func (export "mkp") (result anyref)
                              (struct.new $p (ref.null)))

                             (func (export "mkr") (result anyref)
                              (struct.new $r (ref.null))))`).exports;

    assertEq(typeof ins.mkp().constructor, "function");
    assertErrorMessage(() => new (ins.mkp().constructor)({_0:null}),
                       TypeError,
                       /not constructible/);

    assertEq(typeof ins.mkr().constructor, "function");
    let r = new (ins.mkr().constructor)({_0:null});
    assertEq(typeof r, "object");
}

// MVA v1 restriction: structs that expose ref-typed fields make those fields
// immutable from JS even if we're trying to store the correct type.
//
// However, anyref fields are mutable from JS.

{
    let ins = wasmEvalText(`(module
                             (type $q (struct (field (mut f64))))
                             (type $p (struct (field (mut (ref opt $q))) (field (mut anyref))))

                             (func (export "mkq") (result anyref)
                              (struct.new $q (f64.const 1.5)))

                             (func (export "mkp") (result anyref)
                              (struct.new $p (ref.null) (ref.null))))`).exports;
    let q = ins.mkq();
    assertEq(typeof q, "object");
    assertEq(q._0, 1.5);
    let p = ins.mkp();
    assertEq(typeof p, "object");
    assertEq(p._0, null);

    assertErrorMessage(() => { p._0 = q },
                       Error,
                       /setting immutable field/);

    p._1 = q;
    assertEq(p._1, q);
}

// MVA v1 restriction: structs that expose i64 fields make those fields
// immutable from JS, and the structs are not constructible from JS.

{
    let ins = wasmEvalText(`(module
                             (type $p (struct (field (mut i64))))
                             (func (export "mkp") (result anyref)
                              (struct.new $p (i64.const 0x1234567887654321))))`).exports;

    let p = ins.mkp();
    assertEq(typeof p, "object");
    assertEq(p._0_high, 0x12345678)
    assertEq(p._0_low, 0x87654321|0);

    assertEq(typeof p.constructor, "function");
    assertErrorMessage(() => new (p.constructor)({_0_high:0, _0_low:0}),
                       TypeError,
                       /not constructible/);

    assertErrorMessage(() => { p._0_low = 0 },
                       Error,
                       /setting immutable field/);
    assertErrorMessage(() => { p._0_high = 0 },
                       Error,
                       /setting immutable field/);
}

// A consequence of the current mapping of i64 as two i32 fields is that we run
// a risk of struct.narrow not recognizing the difference.  So check this.

{
    let ins = wasmEvalText(
        `(module
          (type $p (struct (field i64)))
          (type $q (struct (field i32) (field i32)))
          (func $f (param anyref) (result i32)
           (ref.is_null (struct.narrow anyref (ref opt $q) (local.get 0))))
          (func $g (param anyref) (result i32)
           (ref.is_null (struct.narrow anyref (ref opt $p) (local.get 0))))
          (func (export "t1") (result i32)
           (call $f (struct.new $p (i64.const 0))))
          (func (export "t2") (result i32)
           (call $g (struct.new $q (i32.const 0) (i32.const 0)))))`).exports;
    assertEq(ins.t1(), 1);
    assertEq(ins.t2(), 1);
}
