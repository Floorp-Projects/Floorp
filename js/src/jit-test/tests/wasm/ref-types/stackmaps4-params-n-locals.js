// A stress test for stackmap creation as it relates to params and locals.

function Stuff(n) {
    this.n = n;
}
function allocate(n) {
    const res = new Stuff(n);
    // The webassembly loop below will provide us with 254 as an arg after not
    // very long.
    if (n == 254) {
        gc();
    }
    return res;
}

function visit(aStuff) {
    return aStuff.n;
}

let bin = wasmTextToBinary(
`
(module
  (import "" "allocate" (func $allocate (param i32) (result externref)))
  (import "" "visit" (func $visit (param externref) (result i32)))

  ;; A function with many params and locals, most of which are ref-typed.
  ;; The purpose of having so many is to defeat any reasonable attempt at
  ;; allocating them all in registers.  The asymmetrically-placed i32s are
  ;; an attempt to expose any misalignment or inversion of the stack layout
  ;; vs what the stackmap claims the layout to be.

  (func $manyParamsAndLocals (export "manyParamsAndLocals")
    (param $p1 externref) (param $p2 i32)    (param $p3 externref)
    (param $p4 externref) (param $p5 externref) (param $p6 externref)
    (param $p7 externref) (param $p8 externref) (param $p9 i32)
    (result i32)
    (local $l1 externref) (local $l2 externref) (local $l3 externref)
    (local $l4 i32)    (local $l5 externref) (local $l6 i32)
    (local $l7 externref) (local $l8 externref) (local $l9 externref)

    (local $i i32)
    (local $runningTotal i32)

    ;; Bind some objects to l1 .. l9.  The JS harness will already
    ;; have done the same for p1 .. p9.
    (local.set $l1 (call $allocate (i32.const 1)))
    (local.set $l2 (call $allocate (i32.const 3)))
    (local.set $l3 (call $allocate (i32.const 5)))
    (local.set $l4 (i32.const 7))
    (local.set $l5 (call $allocate (i32.const 9)))
    (local.set $l6 (i32.const 11))
    (local.set $l7 (call $allocate (i32.const 13)))
    (local.set $l8 (call $allocate (i32.const 15)))
    (local.set $l9 (call $allocate (i32.const 17)))

    ;; Now loop, allocating as we go, and forcing GC every 256 iterations.
    ;; Also in each iteration, visit all the locals and params, in the hope
    ;; of exposing any cases where they are not held live across GC.
    (loop $CONT
      ;; Allocate, and hold on to the resulting value, so that Ion can't
      ;; delete the allocation.
      (local.set $l9 (call $allocate (i32.and (local.get $i) (i32.const 255))))

      ;; Visit all params and locals

      local.get $runningTotal

      (call $visit (local.get $p1))
      i32.add
      local.get $p2
      i32.add
      (call $visit (local.get $p3))
      i32.add
      (call $visit (local.get $p4))
      i32.add
      (call $visit (local.get $p5))
      i32.add
      (call $visit (local.get $p6))
      i32.add
      (call $visit (local.get $p7))
      i32.add
      (call $visit (local.get $p8))
      i32.add
      local.get $p9
      i32.add

      (call $visit (local.get $l1))
      i32.add
      (call $visit (local.get $l2))
      i32.add
      (call $visit (local.get $l3))
      i32.add
      local.get $l4
      i32.add
      (call $visit (local.get $l5))
      i32.add
      local.get $l6
      i32.add
      (call $visit (local.get $l7))
      i32.add
      (call $visit (local.get $l8))
      i32.add
      (call $visit (local.get $l9))
      i32.add

      local.set $runningTotal

      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $CONT (i32.lt_s (local.get $i) (i32.const 10000)))
    ) ;; loop

    local.get $runningTotal
  ) ;; func
)
`);

let mod = new WebAssembly.Module(bin);
let ins = new WebAssembly.Instance(mod, {"":{allocate, visit}});

let p1 = allocate(97);
let p2 = 93;
let p3 = allocate(91);
let p4 = allocate(83);
let p5 = allocate(79);
let p6 = allocate(73);
let p7 = allocate(71);
let p8 = allocate(67);
let p9 = 61;

let res = ins.exports.manyParamsAndLocals(p1, p2, p3, p4, p5, p6, p7, p8, p9);

// Check that GC didn't mess up p1 .. p9
res += visit(p1);
res += p2;
res += visit(p3);
res += visit(p4);
res += visit(p5);
res += visit(p6);
res += visit(p7);
res += visit(p8);
res += p9;

assertEq(res, 9063795);
