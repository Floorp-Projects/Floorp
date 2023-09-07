// |jit-test| skip-if: !wasmTailCallsEnabled()

wasmValidateText(
    `(module
       (func $arity-1-vs-0
         (i32.const 1) (return_call 1))
       (func))`);

wasmValidateText(
    `(module
       (func $arity-2-vs-0
         (f64.const 2) (i32.const 1) (return_call 1))
       (func))`);

const constants = [{'type': 'i32', 'value': '0x132'},
                   {'type': 'i64', 'value': '0x164'},
                   {'type': 'f32', 'value': '0xf32'},
                   {'type': 'f64', 'value': '0xf64'}];

function validateConst(type, value) {
    wasmValidateText(
        `(module
           (func $const-${type} (result ${type})
             (${type}.const ${value}))
           (func $trampoline-${type} (result ${type})
             (return_call $const-${type}))
           (func $t (result ${type})
             (return_call $trampoline-${type})))`);
}
for (let {type, value} of constants) {
    validateConst(type, value);
}

function validateOneArg(type, value) {
    wasmValidateText(
        `(module
           (func $id-${type} (param ${type}) (result ${type})
             (local.get 0))
           (func $t (result ${type})
             (return_call $id-${type} (${type}.const ${value}))))`);
}
for (let {type, value} of constants) {
    validateOneArg(type, value);
}

function validateTwoArgs(t0, v0, t1, v1) {
    wasmValidateText(
        `(module
           (func $second-${t0}-${t1} (param ${t0} ${t1}) (result ${t1})
             (local.get 1))
           (func $t (result ${t1})
             (return_call $second-${t0}-${t1}
                          (${t0}.const ${v0}) (${t1}.const ${v1}))))`);
}
for (let {type: t0, value: v0} of constants) {
    for (let {type: t1, value: v1} of constants) {
        validateTwoArgs(t0, v0, t1, v1);
    }
}

wasmValidateText(
    `(module
       (func $fac-acc (param i64 i64) (result i64)
         (if (result i64) (i64.eqz (local.get 0))
           (then (local.get 1))
           (else
             (return_call $fac-acc
               (i64.sub (local.get 0) (i64.const 1))
               (i64.mul (local.get 0) (local.get 1))))))
       (func $t (result i64)
         (return_call $fac-acc (i64.const 0) (i64.const 1))))`);

wasmValidateText(
    `(module
       (func $count (param i64) (result i64)
         (if (result i64) (i64.eqz (local.get 0))
           (then (local.get 0))
           (else (return_call $count (i64.sub (local.get 0) (i64.const 1))))))
       (func $t (result i64)
         (return_call $count (i64.const 0))))`);

wasmValidateText(
    `(module
       (func $even (param i64) (result i32)
         (if (result i32) (i64.eqz (local.get 0))
           (then (i32.const 44))
           (else (return_call $odd (i64.sub (local.get 0) (i64.const 1))))))
       (func $odd (param i64) (result i32)
         (if (result i32) (i64.eqz (local.get 0))
           (then (i32.const 99))
           (else (return_call $even (i64.sub (local.get 0) (i64.const 1))))))
       (func $t (result i32)
         (return_call $even (i64.const 0))))`);

wasmFailValidateText(
    `(module
       (func $type-void-vs-num (result i32)
         (return_call 1) (i32.const 0))
       (func))`,
    /type mismatch: expected 1 values, got 0 values/);

wasmFailValidateText(
    `(module
       (func $type-num-vs-num (result i32)
         (return_call 1) (i32.const 0))
       (func (result i64)
         (i64.const 1)))`,
    /expression has type i64 but expected i32/);

wasmFailValidateText(
    `(module
       (func $arity-0-vs-1
         (return_call 1))
       (func (param i32)))`,
    /popping value from empty stack/);

wasmFailValidateText(
    `(module
       (func $arity-0-vs-2
         (return_call 1))
       (func (param f64 i32)))`,
    /popping value from empty stack/);

wasmFailValidateText(
    `(module
       (func $type-first-void-vs-num
         (return_call 1 (nop) (i32.const 1)))
       (func (param i32 i32)))`,
    /popping value from empty stack/);

wasmFailValidateText(
    `(module
       (func $type-second-void-vs-num
         (return_call 1 (i32.const 1) (nop)))
       (func (param i32 i32)))`,
    /popping value from empty stack/);

wasmFailValidateText(
    `(module
       (func $type-first-num-vs-num
         (return_call 1 (f64.const 1) (i32.const 1)))
       (func (param i32 f64)))`,
    /expression has type i32 but expected f64/);

wasmFailValidateText(
    `(module
       (func $type-second-num-vs-num
         (return_call 1 (i32.const 1) (f64.const 1)))
       (func (param f64 i32)))`,
    /expression has type f64 but expected i32/);

wasmFailValidateText(
    `(module
       (func $unbound-func
         (return_call 1)))`,
    /callee index out of range/);

wasmFailValidateText(
    `(module
       (func $large-func
         (return_call 1012321300)))`,
    /callee index out of range/);
