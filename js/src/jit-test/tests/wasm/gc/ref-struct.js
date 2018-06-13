if (!wasmGcEnabled())
    quit(0);

// We'll be running some binary-format tests shortly.

load(libdir + "wasm-binary.js");

const v2vSigSection = sigSection([{args:[], ret:VoidCode}]);

function checkInvalid(body, errorMessage) {
    assertErrorMessage(() => new WebAssembly.Module(
        moduleWithSections([gcFeatureOptInSection(1),
                            v2vSigSection,
                            declSection([0]),
                            bodySection([body])])),
                       WebAssembly.CompileError,
                       errorMessage);
}

// General test case for struct.new, struct.get, and struct.set: binary tree
// manipulation.

{
    let bin = wasmTextToBinary(
        `(module
          (gc_feature_opt_in 1)

          (import $print_lp "" "print_lp" (func))
          (import $print_rp "" "print_rp" (func))
          (import $print_int "" "print_int" (func (param i32)))

          (type $wabbit (struct
                         (field $x (mut i32))
                         (field $left (mut (ref $wabbit)))
                         (field $right (mut (ref $wabbit)))))

          (global $g (mut (ref $wabbit)) (ref.null (ref $wabbit)))

          (global $k (mut i32) (i32.const 0))

          (func (export "init") (param $n i32)
                (set_global $g (call $make (get_local $n))))

          (func $make (param $n i32) (result (ref $wabbit))
                (local $tmp i32)
                (set_local $tmp (get_global $k))
                (set_global $k (i32.add (get_local $tmp) (i32.const 1)))
                (if (ref $wabbit) (i32.le_s (get_local $n) (i32.const 2))
                    (struct.new $wabbit (get_local $tmp) (ref.null (ref $wabbit)) (ref.null (ref $wabbit)))
                    (block (ref $wabbit)
                      (struct.new $wabbit
                                  (get_local $tmp)
                                  (call $make (i32.sub (get_local $n) (i32.const 1)))
                                  (call $make (i32.sub (get_local $n) (i32.const 2)))))))

          (func (export "accumulate") (result i32)
                (call $accum (get_global $g)))

          (func $accum (param $w (ref $wabbit)) (result i32)
                (if i32 (ref.is_null (get_local $w))
                    (i32.const 0)
                    (i32.add (struct.get $wabbit 0 (get_local $w))
                             (i32.sub (call $accum (struct.get $wabbit 1 (get_local $w)))
                                      (call $accum (struct.get $wabbit 2 (get_local $w)))))))

          (func (export "reverse")
                (call $reverse (get_global $g)))

          (func $reverse (param $w (ref $wabbit))
                (local $tmp (ref $wabbit))
                (if (i32.eqz (ref.is_null (get_local $w)))
                    (block
                     (struct.set $wabbit 0 (get_local $w) (i32.mul (i32.const 2) (struct.get $wabbit 0 (get_local $w))))
                     (set_local $tmp (struct.get $wabbit 1 (get_local $w)))
                     (struct.set $wabbit 1 (get_local $w) (struct.get $wabbit 2 (get_local $w)))
                     (struct.set $wabbit 2 (get_local $w) (get_local $tmp))
                     (call $reverse (struct.get $wabbit 1 (get_local $w)))
                     (call $reverse (struct.get $wabbit 2 (get_local $w))))))

          (func (export "print")
                (call $pr (get_global $g)))

          (func $pr (param $w (ref $wabbit))
                (if (i32.eqz (ref.is_null (get_local $w)))
                    (block
                     (call $print_lp)
                     (call $print_int (struct.get $wabbit 0 (get_local $w)))
                     (call $pr (struct.get $wabbit 1 (get_local $w)))
                     (call $pr (struct.get $wabbit 2 (get_local $w)))
                     (call $print_rp))))
         )`);

    let s = "";
    function pr_int(k) { s += k + " "; }
    function pr_lp() { s += "(" };
    function pr_rp() { s += ")" }

    let mod = new WebAssembly.Module(bin);
    let ins = new WebAssembly.Instance(mod, {"":{print_int:pr_int,print_lp:pr_lp,print_rp:pr_rp}}).exports;

    ins.init(6);
    s = ""; ins.print(); assertEq(s, "(0 (1 (2 (3 (4 )(5 ))(6 ))(7 (8 )(9 )))(10 (11 (12 )(13 ))(14 )))");
    assertEq(ins.accumulate(), -13);

    ins.reverse();
    s = ""; ins.print(); assertEq(s, "(0 (20 (28 )(22 (26 )(24 )))(2 (14 (18 )(16 ))(4 (12 )(6 (10 )(8 )))))");
    assertEq(ins.accumulate(), 14);

    for (let i=10; i < 22; i++ ) {
        ins.init(i);
        ins.reverse();
        gc();
        ins.reverse();
    }
}

// Sanity check for struct.set: we /can/ store a (ref T) into a (ref U) field
// with struct.set if T <: U; this should fall out of normal coercion but good
// to test.

wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field (mut (ref $node)))))
      (type $nix (struct (field (mut (ref $node))) (field i32)))
      (func $f (param $p (ref $node)) (param $q (ref $nix))
       (struct.set $node 0 (get_local $p) (get_local $q))))`);

// struct.narrow: if the pointer's null we get null

assertEq(wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (func $f (param $p (ref $node)) (result (ref $node2))
       (struct.narrow (ref $node) (ref $node2) (get_local $p)))
      (func (export "test") (result anyref)
       (call $f (ref.null (ref $node)))))`).exports.test(),
         null);

// struct.narrow: if the downcast succeeds we get the original pointer

assertEq(wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (func $f (param $p (ref $node)) (result (ref $node2))
       (struct.narrow (ref $node) (ref $node2) (get_local $p)))
      (func (export "test") (result i32)
       (local $n (ref $node))
       (set_local $n (struct.new $node2 (i32.const 0) (f32.const 12)))
       (ref.eq (call $f (get_local $n)) (get_local $n))))`).exports.test(),
         1);

// And once more with mutable fields

assertEq(wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field (mut i32))))
      (type $node2 (struct (field (mut i32)) (field f32)))
      (func $f (param $p (ref $node)) (result (ref $node2))
       (struct.narrow (ref $node) (ref $node2) (get_local $p)))
      (func (export "test") (result i32)
       (local $n (ref $node))
       (set_local $n (struct.new $node2 (i32.const 0) (f32.const 12)))
       (ref.eq (call $f (get_local $n)) (get_local $n))))`).exports.test(),
         1);

// A more subtle case: the downcast is to a struct that looks like the original
// struct should succeed because struct.narrow is a structural cast with nominal
// per-field type equality.
//
// We use ref-typed fields here because they have the trickiest equality rules,
// and we have two cases: one where the ref types are the same, and one where
// they reference different structures that look the same; this latter case
// should fail because our structural compatibility is shallow.

assertEq(wasmEvalText(
    `(module
      (gc_feature_opt_in 1)

      (type $node (struct (field i32)))
      (type $node2a (struct (field i32) (field (ref $node))))
      (type $node2b (struct (field i32) (field (ref $node))))

      (func $f (param $p (ref $node)) (result (ref $node2b))
       (struct.narrow (ref $node) (ref $node2b) (get_local $p)))

      (func (export "test") (result i32)
       (local $n (ref $node))
       (set_local $n (struct.new $node2a (i32.const 0) (ref.null (ref $node))))
       (ref.eq (call $f (get_local $n)) (get_local $n))))`).exports.test(),
         1);

assertEq(wasmEvalText(
    `(module
      (gc_feature_opt_in 1)

      (type $node (struct (field i32)))
      (type $nodeCopy (struct (field i32)))
      (type $node2a (struct (field i32) (field (ref $node))))
      (type $node2b (struct (field i32) (field (ref $nodeCopy))))

      (func $f (param $p (ref $node)) (result (ref $node2b))
       (struct.narrow (ref $node) (ref $node2b) (get_local $p)))

      (func (export "test") (result i32)
       (local $n (ref $node))
       (set_local $n (struct.new $node2a (i32.const 0) (ref.null (ref $node))))
       (ref.eq (call $f (get_local $n)) (get_local $n))))`).exports.test(),
         0);

// Another subtle case: struct.narrow can target a type that is not the concrete
// type of the object, but a prefix of that concrete type.

assertEq(wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (type $node3 (struct (field i32) (field f32) (field f64)))
      (func $f (param $p (ref $node)) (result (ref $node2))
       (struct.narrow (ref $node) (ref $node2) (get_local $p)))
      (func (export "test") (result i32)
       (local $n (ref $node))
       (set_local $n (struct.new $node3 (i32.const 0) (f32.const 12) (f64.const 17)))
       (ref.eq (call $f (get_local $n)) (get_local $n))))`).exports.test(),
         1);

// struct.narrow: if the downcast fails we get null

assertEq(wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (type $snort (struct (field i32) (field f64)))
      (func $f (param $p (ref $node)) (result (ref $node2))
       (struct.narrow (ref $node) (ref $node2) (get_local $p)))
      (func (export "test") (result anyref)
       (call $f (struct.new $snort (i32.const 0) (f64.const 12)))))`).exports.test(),
         null);

// struct.narrow: anyref -> struct when the anyref is the right struct;
// special case since anyref requires unboxing

assertEq(wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (func $f (param $p anyref) (result (ref $node))
       (struct.narrow anyref (ref $node) (get_local $p)))
      (func (export "test") (result i32)
       (local $n (ref $node))
       (set_local $n (struct.new $node (i32.const 0)))
       (ref.eq (call $f (get_local $n)) (get_local $n))))`).exports.test(),
         1);

// struct.narrow: anyref -> struct when the anyref is some random gunk.

assertEq(wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (func (export "test") (param $p anyref) (result anyref)
       (struct.narrow anyref (ref $node) (get_local $p))))`).exports.test({hi:37}),
         null);

// Types are private to an instance and struct.narrow can't break this

{
    let txt =
        `(module
          (gc_feature_opt_in 1)
          (type $node (struct (field i32)))
          (func (export "make") (param $n i32) (result anyref)
           (struct.new $node (get_local $n)))
          (func (export "coerce") (param $p anyref) (result i32)
           (ref.is_null (struct.narrow anyref (ref $node) (get_local $p)))))`;
    let mod = new WebAssembly.Module(wasmTextToBinary(txt));
    let ins1 = new WebAssembly.Instance(mod).exports;
    let ins2 = new WebAssembly.Instance(mod).exports;
    let obj = ins1.make(37);
    assertEq(obj._0, 37);
    assertEq(ins2.coerce(obj), 1);
}

// Negative tests

// Attempting to mutate immutable field with struct.set

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (func $f (param $p (ref $node))
       (struct.set $node 0 (get_local $p) (i32.const 37))))`),
                   WebAssembly.CompileError,
                   /field is not mutable/);

// Attempting to store incompatible value in mutable field with struct.set

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field (mut i32))))
      (func $f (param $p (ref $node))
       (struct.set $node 0 (get_local $p) (f32.const 37))))`),
                   WebAssembly.CompileError,
                   /expression has type f32 but expected i32/);

// Out-of-bounds reference for struct.get

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (func $f (param $p (ref $node)) (result i32)
       (struct.get $node 1 (get_local $p))))`),
                   WebAssembly.CompileError,
                   /field index out of range/);

// Out-of-bounds reference for struct.set

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field (mut i32))))
      (func $f (param $p (ref $node))
       (struct.set $node 1 (get_local $p) (i32.const 37))))`),
                   WebAssembly.CompileError,
                   /field index out of range/);

// Base pointer is of unrelated type to stated type in struct.get

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (type $snort (struct (field f64)))
      (func $f (param $p (ref $snort)) (result i32)
       (struct.get $node 0 (get_local $p))))`),
                   WebAssembly.CompileError,
                   /expression has type.*but expected.*/);

// Base pointer is of unrelated type to stated type in struct.set

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field (mut i32))))
      (type $snort (struct (field f64)))
      (func $f (param $p (ref $snort)) (result i32)
       (struct.set $node 0 (get_local $p) (i32.const 0))))`),
                   WebAssembly.CompileError,
                   /expression has type.*but expected.*/);

// Base pointer is of unrelated type to stated type in struct.narrow

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (type $snort (struct (field f64)))
      (func $f (param $p (ref $snort)) (result (ref $node2))
       (struct.narrow (ref $node) (ref $node2) (get_local 0))))`),
                   WebAssembly.CompileError,
                   /expression has type.*but expected.*/);

// source and target types are compatible except for mutability

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (type $node2 (struct (field (mut i32)) (field f32)))
      (func $f (param $p (ref $node)) (result (ref $node2))
       (struct.narrow (ref $node) (ref $node2) (get_local 0))))`),
                   WebAssembly.CompileError,
                   /invalid narrowing operation/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field (mut i32))))
      (type $node2 (struct (field i32) (field f32)))
      (func $f (param $p (ref $node)) (result (ref $node2))
       (struct.narrow (ref $node) (ref $node2) (get_local 0))))`),
                   WebAssembly.CompileError,
                   /invalid narrowing operation/);

// source and target types must be ref types: source syntax

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (func $f (param $p (ref $node)) (result anyref)
       (struct.narrow i32 anyref (get_local 0))))`),
                   SyntaxError,
                   /struct.narrow requires ref type/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (func $f (param $p (ref $node)) (result anyref)
       (struct.narrow anyref i32 (get_local 0))))`),
                   SyntaxError,
                   /struct.narrow requires ref type/);

// source and target types must be ref types: binary format

checkInvalid(funcBody({locals:[],
                       body:[
                           RefNull,
                           AnyrefCode,
                           MiscPrefix, StructNarrow, I32Code, AnyrefCode,
                           DropCode
                       ]}),
             /invalid reference type for struct.narrow/);

checkInvalid(funcBody({locals:[],
                       body:[
                           RefNull,
                           AnyrefCode,
                           MiscPrefix, StructNarrow, AnyrefCode, I32Code,
                           DropCode
                       ]}),
             /invalid reference type for struct.narrow/);

// target type is anyref so source type must be anyref as well (no upcasts)

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (func $f (param $p (ref $node)) (result anyref)
       (struct.narrow (ref $node) anyref (get_local 0))))`),
                   WebAssembly.CompileError,
                   /invalid type combination in struct.narrow/);

// target type must be subtype of source type (no upcasts)

assertErrorMessage(() => wasmEvalText(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (func $f (param $p (ref $node2)) (result anyref)
       (struct.narrow (ref $node2) (ref $node) (get_local 0))))`),
                   WebAssembly.CompileError,
                   /invalid narrowing operation/);

// Null pointer dereference in struct.get

assertErrorMessage(function() {
    let ins = wasmEvalText(
        `(module
          (gc_feature_opt_in 1)
          (type $node (struct (field i32)))
          (func (export "test")
           (drop (call $f (ref.null (ref $node)))))
          (func $f (param $p (ref $node)) (result i32)
           (struct.get $node 0 (get_local $p))))`);
    ins.exports.test();
},
                   WebAssembly.RuntimeError,
                   /dereferencing null pointer/);

// Null pointer dereference in struct.set

assertErrorMessage(function() {
    let ins = wasmEvalText(
        `(module
          (gc_feature_opt_in 1)
          (type $node (struct (field (mut i32))))
          (func (export "test")
           (call $f (ref.null (ref $node))))
          (func $f (param $p (ref $node))
           (struct.set $node 0 (get_local $p) (i32.const 0))))`);
    ins.exports.test();
},
                   WebAssembly.RuntimeError,
                   /dereferencing null pointer/);
