// |jit-test| skip-if: !wasmGcEnabled()

// We can read the object fields from JS

{
    let ins = wasmEvalText(`(module
                             (type $p (struct (field f64) (field (mut i32))))

                             (func (export "mkp") (result eqref)
                              (struct.new_with_rtt $p (f64.const 1.5) (i32.const 33) (rtt.canon $p))))`).exports;

    let p = ins.mkp();
    assertEq(p[0], 1.5);
    assertEq(p[1], 33);
    assertEq(p[2], undefined);
}

// Writing an immutable field from JS throws.

{
    let ins = wasmEvalText(`(module
                             (type $p (struct (field f64)))

                             (func (export "mkp") (result eqref)
                              (struct.new_with_rtt $p (f64.const 1.5) (rtt.canon $p))))`).exports;

    let p = ins.mkp();
    assertErrorMessage(() => p[0] = 5.7,
                       Error,
                       /setting immutable field/);
}

// MVA v1 restriction: structs are not constructible from JS.

{
    let ins = wasmEvalText(`(module
                             (type $q (struct (field (mut f64))))
                             (type $p (struct (field (mut (ref null $q)))))

                             (type $r (struct (field (mut eqref))))

                             (func (export "mkp") (result eqref)
                              (struct.new_with_rtt $p (ref.null $q) (rtt.canon $p)))

                             (func (export "mkr") (result eqref)
                              (struct.new_with_rtt $r (ref.null eq) (rtt.canon $r))))`).exports;

    assertEq(ins.mkp().constructor, Object);
    assertEq(ins.mkr().constructor, Object);
}

// MVA v1 restriction: all fields are immutable

{
    let ins = wasmEvalText(`(module
                             (type $q (struct (field (mut f64))))
                             (type $p (struct (field (mut (ref null $q))) (field (mut eqref))))

                             (func (export "mkq") (result eqref)
                              (struct.new_with_rtt $q (f64.const 1.5) (rtt.canon $q)))

                             (func (export "mkp") (result eqref)
                              (struct.new_with_rtt $p (ref.null $q) (ref.null eq) (rtt.canon $p))))`).exports;
    let q = ins.mkq();
    assertEq(typeof q, "object");
    assertEq(q[0], 1.5);

    let p = ins.mkp();
    assertEq(typeof p, "object");
    assertEq(p[0], null);

    assertErrorMessage(() => { p[0] = q },
                       Error,
                       /setting immutable field/);

    assertErrorMessage(() => { p[1] = q },
                       Error,
                       /setting immutable field/);
}

// MVA v1 restriction: structs that expose i64 fields make those fields
// immutable from JS, and the structs are not constructible from JS.

{
    let ins = wasmEvalText(`(module
                             (type $p (struct (field (mut i64))))
                             (func (export "mkp") (result eqref)
                              (struct.new_with_rtt $p (i64.const 0x1234567887654321) (rtt.canon $p))))`).exports;

    let p = ins.mkp();
    assertEq(typeof p, "object");
    assertEq(p[0], 0x1234567887654321n)

    assertEq(p.constructor, Object);

    assertErrorMessage(() => { p[0] = 0 },
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
          (func $f (param eqref) (result i32)
           local.get 0
           rtt.canon $q
           ref.test eq $q
          )
          (func $g (param eqref) (result i32)
           local.get 0
           rtt.canon $p
           ref.test eq $p
          )
          (func (export "t1") (result i32)
           (call $f (struct.new_with_rtt $p (i64.const 0) (rtt.canon $p))))
          (func (export "t2") (result i32)
           (call $g (struct.new_with_rtt $q (i32.const 0) (i32.const 0) (rtt.canon $q)))))`).exports;
    assertEq(ins.t1(), 0);
    assertEq(ins.t2(), 0);
}
