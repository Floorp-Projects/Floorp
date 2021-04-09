// Tests wasm frame tracing.  Only tests for direct and indirect call chains
// in wasm that lead to JS allocation.  Does not test any timeout or interrupt
// related aspects.  The structure is
//
//   test top level: call fn2
//   fn2: call fn1
//   fn1: do 100k times { call-direct fn0; call-indirect fn0; }
//   fn0: call out to JS that does allocation
//
// Eventually fn0 will trigger GC and we expect the chain of resulting frames
// to be traced correctly.  fn2, fn1 and fn0 have some ref-typed args, so
// there will be traceable stack words to follow, in the sequence of frames.

const {Module,Instance} = WebAssembly;

let t =
  `(module
     (import "" "check3" (func $check3 (param externref) (param externref) (param externref)))
     (type $typeOfFn0
           (func (param i32) (param externref) (param i32)
                 (param externref) (param externref) (param i32) (result i32)))
     (table 1 1 funcref)
     (elem (i32.const 0) $fn0)

     (import "" "alloc" (func $alloc (result externref)))

     ;; -- fn 0
     (func $fn0 (export "fn0")
                 (param $arg1 i32) (param $arg2 externref) (param $arg3 i32)
                 (param $arg4 externref) (param $arg5 externref) (param $arg6 i32) (result i32)
       (call $alloc)
       drop
       (i32.add (i32.add (local.get $arg1) (local.get $arg3)) (local.get $arg6))

       ;; Poke the ref-typed arguments, to be sure that they got kept alive
       ;; properly across any GC that the |alloc| call might have done.
       (call $check3 (local.get $arg2) (local.get $arg4) (local.get $arg5))
     )

     ;; -- fn 1
     (func $fn1 (export "fn1") (param $arg1 externref) (result i32)
       (local $i i32)

       (loop (result i32)
         ;; call direct 0
         (call $fn0 (i32.const 10) (local.get $arg1) (i32.const 12)
                    (local.get $arg1) (local.get $arg1) (i32.const 15))

         ;; call indirect 0
         (call_indirect (type $typeOfFn0)
                    (i32.const 10) (local.get $arg1) (i32.const 12)
                    (local.get $arg1) (local.get $arg1) (i32.const 15)
                    (i32.const 0)) ;; table index

         i32.add

         ;; Do 60k iterations of this loop, to get a good amount of allocation
         (local.set $i (i32.add (local.get $i) (i32.const 1)))
         (br_if 0 (i32.lt_s (local.get $i) (i32.const 60000)))
       )
     )

     ;; -- fn 2
     (func $fn2 (export "fn2") (param $arg1 externref) (result i32)
       (call $fn1 (local.get $arg1))
     )
   )`;

function Croissant(chocolate, number) {
    this.chocolate = chocolate;
    this.number = number;
}

function allocates() {
    return new Croissant(true, 271828);
}

function check3(a1, a2, a3) {
    assertEq(a1.number, 31415927);
    assertEq(a2.number, 31415927);
    assertEq(a3.number, 31415927);
}

let i = wasmEvalText(t, {"":{alloc: allocates, check3: check3}});

print(i.exports.fn2( new Croissant(false, 31415927) ));
