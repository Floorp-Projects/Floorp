// |jit-test| skip-if: !wasmGcEnabled() || wasmCompileMode() != 'baseline'

// We'll be running some binary-format tests shortly.

load(libdir + "wasm-binary.js");

const v2vSigSection = sigSection([{args:[], ret:VoidCode}]);

function checkInvalid(body, errorMessage) {
    assertErrorMessage(() => new WebAssembly.Module(
        moduleWithSections([v2vSigSection,
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
          (import "" "print_lp" (func $print_lp))
          (import "" "print_rp" (func $print_rp))
          (import "" "print_int" (func $print_int (param i32)))

          (type $wabbit (struct
                         (field $x (mut i32))
                         (field $left (mut (ref opt $wabbit)))
                         (field $right (mut (ref opt $wabbit)))))

          (global $g (mut (ref opt $wabbit)) (ref.null))

          (global $k (mut i32) (i32.const 0))

          (func (export "init") (param $n i32)
                (global.set $g (call $make (local.get $n))))

          (func $make (param $n i32) (result (ref opt $wabbit))
                (local $tmp i32)
                (local.set $tmp (global.get $k))
                (global.set $k (i32.add (local.get $tmp) (i32.const 1)))
                (if (result (ref opt $wabbit)) (i32.le_s (local.get $n) (i32.const 2))
                    (struct.new $wabbit (local.get $tmp) (ref.null) (ref.null))
                    (block (result (ref opt $wabbit))
                      (struct.new $wabbit
                                  (local.get $tmp)
                                  (call $make (i32.sub (local.get $n) (i32.const 1)))
                                  (call $make (i32.sub (local.get $n) (i32.const 2)))))))

          (func (export "accumulate") (result i32)
                (call $accum (global.get $g)))

          (func $accum (param $w (ref opt $wabbit)) (result i32)
                (if (result i32) (ref.is_null (local.get $w))
                    (i32.const 0)
                    (i32.add (struct.get $wabbit 0 (local.get $w))
                             (i32.sub (call $accum (struct.get $wabbit 1 (local.get $w)))
                                      (call $accum (struct.get $wabbit 2 (local.get $w)))))))

          (func (export "reverse")
                (call $reverse (global.get $g)))

          (func $reverse (param $w (ref opt $wabbit))
                (local $tmp (ref opt $wabbit))
                (if (i32.eqz (ref.is_null (local.get $w)))
                    (block
                     (struct.set $wabbit 0 (local.get $w) (i32.mul (i32.const 2) (struct.get $wabbit 0 (local.get $w))))
                     (local.set $tmp (struct.get $wabbit 1 (local.get $w)))
                     (struct.set $wabbit 1 (local.get $w) (struct.get $wabbit 2 (local.get $w)))
                     (struct.set $wabbit 2 (local.get $w) (local.get $tmp))
                     (call $reverse (struct.get $wabbit 1 (local.get $w)))
                     (call $reverse (struct.get $wabbit 2 (local.get $w))))))

          (func (export "print")
                (call $pr (global.get $g)))

          (func $pr (param $w (ref opt $wabbit))
                (if (i32.eqz (ref.is_null (local.get $w)))
                    (block
                     (call $print_lp)
                     (call $print_int (struct.get $wabbit 0 (local.get $w)))
                     (call $pr (struct.get $wabbit 1 (local.get $w)))
                     (call $pr (struct.get $wabbit 2 (local.get $w)))
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

// Sanity check for struct.set: we /can/ store a (ref opt T) into a (ref opt U) field
// with struct.set if T <: U; this should fall out of normal coercion but good
// to test.

wasmEvalText(
    `(module
      (type $node (struct (field (mut (ref opt $node)))))
      (type $nix (struct (field (mut (ref opt $node))) (field i32)))
      (func $f (param $p (ref opt $node)) (param $q (ref opt $nix))
       (struct.set $node 0 (local.get $p) (local.get $q))))`);

// struct.narrow: if the pointer's null we get null

assertEq(wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (func $f (param $p (ref opt $node)) (result (ref opt $node2))
       (struct.narrow (ref opt $node) (ref opt $node2) (local.get $p)))
      (func (export "test") (result anyref)
       (call $f (ref.null))))`).exports.test(),
         null);

// struct.narrow: if the downcast succeeds we get the original pointer

assertEq(wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (func $f (param $p (ref opt $node)) (result (ref opt $node2))
       (struct.narrow (ref opt $node) (ref opt $node2) (local.get $p)))
      (func (export "test") (result i32)
       (local $n (ref opt $node))
       (local.set $n (struct.new $node2 (i32.const 0) (f32.const 12)))
       (ref.eq (call $f (local.get $n)) (local.get $n))))`).exports.test(),
         1);

// And once more with mutable fields

assertEq(wasmEvalText(
    `(module
      (type $node (struct (field (mut i32))))
      (type $node2 (struct (field (mut i32)) (field f32)))
      (func $f (param $p (ref opt $node)) (result (ref opt $node2))
       (struct.narrow (ref opt $node) (ref opt $node2) (local.get $p)))
      (func (export "test") (result i32)
       (local $n (ref opt $node))
       (local.set $n (struct.new $node2 (i32.const 0) (f32.const 12)))
       (ref.eq (call $f (local.get $n)) (local.get $n))))`).exports.test(),
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
      (type $node (struct (field i32)))
      (type $node2a (struct (field i32) (field (ref opt $node))))
      (type $node2b (struct (field i32) (field (ref opt $node))))

      (func $f (param $p (ref opt $node)) (result (ref opt $node2b))
       (struct.narrow (ref opt $node) (ref opt $node2b) (local.get $p)))

      (func (export "test") (result i32)
       (local $n (ref opt $node))
       (local.set $n (struct.new $node2a (i32.const 0) (ref.null)))
       (ref.eq (call $f (local.get $n)) (local.get $n))))`).exports.test(),
         1);

assertEq(wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (type $nodeCopy (struct (field i32)))
      (type $node2a (struct (field i32) (field (ref opt $node))))
      (type $node2b (struct (field i32) (field (ref opt $nodeCopy))))

      (func $f (param $p (ref opt $node)) (result (ref opt $node2b))
       (struct.narrow (ref opt $node) (ref opt $node2b) (local.get $p)))

      (func (export "test") (result i32)
       (local $n (ref opt $node))
       (local.set $n (struct.new $node2a (i32.const 0) (ref.null)))
       (ref.eq (call $f (local.get $n)) (local.get $n))))`).exports.test(),
         0);

// Another subtle case: struct.narrow can target a type that is not the concrete
// type of the object, but a prefix of that concrete type.

assertEq(wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (type $node3 (struct (field i32) (field f32) (field f64)))
      (func $f (param $p (ref opt $node)) (result (ref opt $node2))
       (struct.narrow (ref opt $node) (ref opt $node2) (local.get $p)))
      (func (export "test") (result i32)
       (local $n (ref opt $node))
       (local.set $n (struct.new $node3 (i32.const 0) (f32.const 12) (f64.const 17)))
       (ref.eq (call $f (local.get $n)) (local.get $n))))`).exports.test(),
         1);

// struct.narrow: if the downcast fails we get null

assertEq(wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (type $snort (struct (field i32) (field f64)))
      (func $f (param $p (ref opt $node)) (result (ref opt $node2))
       (struct.narrow (ref opt $node) (ref opt $node2) (local.get $p)))
      (func (export "test") (result anyref)
       (call $f (struct.new $snort (i32.const 0) (f64.const 12)))))`).exports.test(),
         null);

// struct.narrow: anyref -> struct when the anyref is the right struct;
// special case since anyref requires unboxing

assertEq(wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (func $f (param $p anyref) (result (ref opt $node))
       (struct.narrow anyref (ref opt $node) (local.get $p)))
      (func (export "test") (result i32)
       (local $n (ref opt $node))
       (local.set $n (struct.new $node (i32.const 0)))
       (ref.eq (call $f (local.get $n)) (local.get $n))))`).exports.test(),
         1);

// struct.narrow: anyref -> struct when the anyref is some random gunk.

assertEq(wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (func (export "test") (param $p anyref) (result anyref)
       (struct.narrow anyref (ref opt $node) (local.get $p))))`).exports.test({hi:37}),
         null);

// Types are private to an instance and struct.narrow can't break this

{
    let txt =
        `(module
          (type $node (struct (field i32)))
          (func (export "make") (param $n i32) (result anyref)
           (struct.new $node (local.get $n)))
          (func (export "coerce") (param $p anyref) (result i32)
           (ref.is_null (struct.narrow anyref (ref opt $node) (local.get $p)))))`;
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
      (type $node (struct (field i32)))
      (func $f (param $p (ref opt $node))
       (struct.set $node 0 (local.get $p) (i32.const 37))))`),
                   WebAssembly.CompileError,
                   /field is not mutable/);

// Attempting to store incompatible value in mutable field with struct.set

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field (mut i32))))
      (func $f (param $p (ref opt $node))
       (struct.set $node 0 (local.get $p) (f32.const 37))))`),
                   WebAssembly.CompileError,
                   /expression has type f32 but expected i32/);

// Out-of-bounds reference for struct.get

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (func $f (param $p (ref opt $node)) (result i32)
       (struct.get $node 1 (local.get $p))))`),
                   WebAssembly.CompileError,
                   /field index out of range/);

// Out-of-bounds reference for struct.set

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field (mut i32))))
      (func $f (param $p (ref opt $node))
       (struct.set $node 1 (local.get $p) (i32.const 37))))`),
                   WebAssembly.CompileError,
                   /field index out of range/);

// Base pointer is of unrelated type to stated type in struct.get

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (type $snort (struct (field f64)))
      (func $f (param $p (ref opt $snort)) (result i32)
       (struct.get $node 0 (local.get $p))))`),
                   WebAssembly.CompileError,
                   /expression has type.*but expected.*/);

// Base pointer is of unrelated type to stated type in struct.set

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field (mut i32))))
      (type $snort (struct (field f64)))
      (func $f (param $p (ref opt $snort)) (result i32)
       (struct.set $node 0 (local.get $p) (i32.const 0))))`),
                   WebAssembly.CompileError,
                   /expression has type.*but expected.*/);

// Base pointer is of unrelated type to stated type in struct.narrow

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (type $snort (struct (field f64)))
      (func $f (param $p (ref opt $snort)) (result (ref opt $node2))
       (struct.narrow (ref opt $node) (ref opt $node2) (local.get 0))))`),
                   WebAssembly.CompileError,
                   /expression has type.*but expected.*/);

// source and target types are compatible except for mutability

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (type $node2 (struct (field (mut i32)) (field f32)))
      (func $f (param $p (ref opt $node)) (result (ref opt $node2))
       (struct.narrow (ref opt $node) (ref opt $node2) (local.get 0))))`),
                   WebAssembly.CompileError,
                   /invalid narrowing operation/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field (mut i32))))
      (type $node2 (struct (field i32) (field f32)))
      (func $f (param $p (ref opt $node)) (result (ref opt $node2))
       (struct.narrow (ref opt $node) (ref opt $node2) (local.get 0))))`),
                   WebAssembly.CompileError,
                   /invalid narrowing operation/);

// source and target types must be ref types: source syntax

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (func $f (param $p (ref opt $node)) (result anyref)
       (struct.narrow i32 anyref (local.get 0))))`),
                   WebAssembly.CompileError,
                   /invalid reference type/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (func $f (param $p (ref opt $node)) (result anyref)
       (struct.narrow anyref i32 (local.get 0))))`),
                   WebAssembly.CompileError,
                   /invalid reference type/);

// source and target types must be ref types: binary format

checkInvalid(funcBody({locals:[],
                       body:[
                           RefNullCode,
                           GcPrefix, StructNarrow, I32Code, AnyrefCode,
                           DropCode
                       ]}),
             /invalid reference type for struct.narrow/);

checkInvalid(funcBody({locals:[],
                       body:[
                           RefNullCode,
                           GcPrefix, StructNarrow, AnyrefCode, I32Code,
                           DropCode
                       ]}),
             /invalid reference type for struct.narrow/);

// target type is anyref so source type must be anyref as well (no upcasts)

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (func $f (param $p (ref opt $node)) (result anyref)
       (struct.narrow (ref opt $node) anyref (local.get 0))))`),
                   WebAssembly.CompileError,
                   /invalid type combination in struct.narrow/);

// target type must be subtype of source type (no upcasts)

assertErrorMessage(() => wasmEvalText(
    `(module
      (type $node (struct (field i32)))
      (type $node2 (struct (field i32) (field f32)))
      (func $f (param $p (ref opt $node2)) (result anyref)
       (struct.narrow (ref opt $node2) (ref opt $node) (local.get 0))))`),
                   WebAssembly.CompileError,
                   /invalid narrowing operation/);

// Null pointer dereference in struct.get

assertErrorMessage(function() {
    let ins = wasmEvalText(
        `(module
          (type $node (struct (field i32)))
          (func (export "test")
           (drop (call $f (ref.null))))
          (func $f (param $p (ref opt $node)) (result i32)
           (struct.get $node 0 (local.get $p))))`);
    ins.exports.test();
},
                   WebAssembly.RuntimeError,
                   /dereferencing null pointer/);

// Null pointer dereference in struct.set

assertErrorMessage(function() {
    let ins = wasmEvalText(
        `(module
          (type $node (struct (field (mut i32))))
          (func (export "test")
           (call $f (ref.null)))
          (func $f (param $p (ref opt $node))
           (struct.set $node 0 (local.get $p) (i32.const 0))))`);
    ins.exports.test();
},
                   WebAssembly.RuntimeError,
                   /dereferencing null pointer/);
