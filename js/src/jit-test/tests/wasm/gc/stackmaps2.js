// |jit-test| skip-if: !wasmReftypesEnabled()

// Tests wasm frame tracing in the presence of interrupt handlers that perform
// allocation.  The structure is
//
//   test top level: call fn2
//   fn2: call fn1
//   fn1: repeat { call-direct fn0; call-indirect fn0; }
//   fn0: a 100-iteration loop that does nothing except waste time
//
// At the same time we are asynchronously runnning handler(), which does a lot
// of allocation.  At some point that will trigger a GC.  Assuming that
// handler() runs whilst fn0 is running (the most likely scenario, since fn0
// consumes the majority of the wasm running time), then the runtime will walk
// the stack from the wasm exit frame, through fn0, fn1 and finally fn2.  As
// with stackmaps1.js, there are some ref-typed args in use so as provide
// traceable stack slots to follow.
//
// The test runs until the loop in fn1 determines that handler() has allocated
// sufficient memory as to have caused at least three collections.  This helps
// keep the test effective in the face of wide variations in the rate of
// progress of the handler()'s loop (eg x86+native is fast, arm64+simulator is
// slow).

const {Module,Instance} = WebAssembly;

const DEBUG = false;

let t =
  `(module
     (type $typeOfFn0
           (func (result i32) (param i32) (param anyref) (param i32)
                              (param anyref) (param anyref) (param i32)))
     (table 1 1 funcref)
     (elem (i32.const 0) $fn0)

     (import $alloc "" "alloc" (func (result anyref)))
     (import $quitp "" "quitp" (func (result i32)))
     (import $check3 "" "check3" (func (param anyref) (param anyref) (param anyref)))

     ;; -- fn 0
     (func $fn0 (export "fn0")
                (result i32) (param $arg1 i32) (param $arg2 anyref) (param $arg3 i32)
                             (param $arg4 anyref) (param $arg5 anyref) (param $arg6 i32)
       (local $i i32)

       ;; spinloop to waste time
       (loop
         (local.set $i (i32.add (local.get $i) (i32.const 1)))
         (br_if 0 (i32.lt_s (local.get $i) (i32.const 100)))
       )

       (i32.add (i32.add (local.get $arg1) (local.get $arg3)) (local.get $arg6))

       ;; Poke the ref-typed arguments, to be sure that they got kept alive
       ;; properly across any GC that might have happened.
       (call $check3 (local.get $arg2) (local.get $arg4) (local.get $arg5))
     )

     ;; -- fn 1
     (func $fn1 (export "fn1") (param $arg1 anyref) (result i32)
       (loop i32
         ;; call direct to $fn0
         (call $fn0 (i32.const 10) (local.get $arg1) (i32.const 12)
                    (local.get $arg1) (local.get $arg1) (i32.const 15))

         ;; call indirect to table index 0, which is $fn0
         (call_indirect $typeOfFn0
                    (i32.const 10) (local.get $arg1) (i32.const 12)
                    (local.get $arg1) (local.get $arg1) (i32.const 15)
                    (i32.const 0)) ;; table index

         i32.add

         ;; Continue iterating until handler() has allocated enough
         (br_if 0 (i32.eq (call $quitp) (i32.const 0)))
       )
     )

     ;; -- fn 2
     (func $fn2 (export "fn2") (param $arg1 anyref) (result i32)
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

let totAllocs = 0;

function handler() {
    if (DEBUG) {
        print('XXXXXXXX icallback: START');
    }
    let q = allocates();
    let sum = 0;
    let iters = 15000;
    for (let i = 0; i < iters; i++) {
        let x = allocates();
        // Without this hoop jumping to create an apparent use of |x|, Ion
        // will remove the allocation call and make the test pointless.
        if (x == q) { sum++; }
    }
    totAllocs += iters;
    // Artificial use of |sum|.  See comment above.
    if (sum == 133713371337) { print("unlikely!"); }
    timeout(0.5, handler);
    if (DEBUG) {
        print('XXXXXXXX icallback: END');
    }
    return true;
}

function quitp() {
    return totAllocs > 200000 ? 1 : 0;
}

function check3(a1, a2, a3) {
    assertEq(a1.number, 31415927);
    assertEq(a2.number, 31415927);
    assertEq(a3.number, 31415927);
}

let i = wasmEvalText(t, {"":{alloc: allocates, quitp: quitp, check3: check3}});

timeout(0.5, handler);
print(i.exports.fn2( new Croissant(false, 31415927) ));
