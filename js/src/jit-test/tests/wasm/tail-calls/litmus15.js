// |jit-test| skip-if: !wasmTailCallsEnabled()

// On systems with up to four register arguments, this alternately grows and then shrinks
// the stack frame across tail call boundaries.  All functions are in the same
// module.

var ins = wasmEvalText(`
(module
  (type $ty0 (func (result i32)))
  (type $ty1 (func (param i32) (result i32)))
  (type $ty2 (func (param i32 i32) (result i32)))
  (type $ty3 (func (param i32 i32 i32) (result i32)))
  (type $ty4 (func (param i32 i32 i32 i32) (result i32)))
  (type $ty5 (func (param i32 i32 i32 i32 i32) (result i32)))
  (type $ty6 (func (param i32 i32 i32 i32 i32 i32) (result i32)))
  (type $ty7 (func (param i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (type $ty8 (func (param i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (type $ty9 (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))

  (table $t 10 10 funcref)
  (elem (table $t) (i32.const 0) func $f $g $h $i $j $k $l $m $n $o)

  (global $glob (mut i32) (i32.const ${TailCallIterations}))

  (func $o (param i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    (if (result i32) (i32.eqz (global.get $glob))
        (then
          (i32.or (i32.const 512)
            (i32.or (local.get 0)
                (i32.or (local.get 1)
                    (i32.or (local.get 2)
                        (i32.or (local.get 3)
                            (i32.or (local.get 4)
                                (i32.or (local.get 5)
                                    (i32.or (local.get 6)
                                        (i32.or (local.get 7) (local.get 8)))))))))))
        (else
          (block (result i32)
            (global.set $glob (i32.sub (global.get $glob) (i32.const 1)))
            (return_call_indirect (type $ty0) (i32.const 0))))))

  (func $n (param i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    (return_call_indirect (type $ty9) (i32.const 256) (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4) (local.get 5) (local.get 6) (local.get 7) (i32.const 9)))

  (func $m (param i32 i32 i32 i32 i32 i32 i32) (result i32)
    (return_call_indirect (type $ty8) (i32.const 128) (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4) (local.get 5) (local.get 6) (i32.const 8)))

  (func $l (param i32 i32 i32 i32 i32 i32) (result i32)
    (return_call_indirect (type $ty7) (i32.const 64) (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4) (local.get 5) (i32.const 7)))

  (func $k (param i32 i32 i32 i32 i32) (result i32)
    (return_call_indirect (type $ty6) (i32.const 32) (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4) (i32.const 6)))

  (func $j (param i32 i32 i32 i32) (result i32)
    (return_call_indirect (type $ty5) (i32.const 16) (local.get 0) (local.get 1) (local.get 2) (local.get 3) (i32.const 5)))

  (func $i (param i32 i32 i32) (result i32)
    (return_call_indirect (type $ty4) (i32.const 8) (local.get 0) (local.get 1) (local.get 2) (i32.const 4)))

  (func $h (param i32 i32) (result i32)
    (return_call_indirect (type $ty3) (i32.const 4) (local.get 0) (local.get 1) (i32.const 3)))

  (func $g (param i32) (result i32)
    (return_call_indirect (type $ty2) (i32.const 2) (local.get 0) (i32.const 2)))

  (func $f (export "f") (result i32)
    (return_call_indirect (type $ty1) (i32.const 1) (i32.const 1))))`);

assertEq(ins.exports.f(), 1023);
print("OK")


