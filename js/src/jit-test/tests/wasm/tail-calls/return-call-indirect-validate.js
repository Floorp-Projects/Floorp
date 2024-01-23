// |jit-test| skip-if: !wasmTailCallsEnabled()

wasmValidateText(
    `(module
       (type (func))
       (table 0 anyfunc)
       (func $arity-1-vs-0
         (return_call_indirect (type 0)
                               (i32.const 1) (i32.const 0))))`);

wasmValidateText(
    `(module
       (type (func))
       (table 0 anyfunc)
       (func $arity-1-vs-0
         (return_call_indirect (type 0)
                               (f64.const 2) (i32.const 1) (i32.const 0))))`);

const constants = [{'type': 'i32', 'value': '0x132'},
                   {'type': 'i64', 'value': '0x164'},
                   {'type': 'f32', 'value': '0xf32'},
                   {'type': 'f64', 'value': '0xf64'}];

function validateConst(type, value) {
    wasmValidateText(
        `(module
           (type $out-${type} (func (result ${type})))
           (func $type-${type} (result ${type})
             (return_call_indirect (type $out-${type}) (i32.const 0)))
           (func $const-${type} (result ${type})
             (${type}.const ${value}))
           (func $t (result ${type})
             (return_call $type-${type}))
           (table anyfunc (elem $const-${type})))`);
}
for (let {type, value} of constants) {
    validateConst(type, value);
}

function validateOneArg(type, value) {
    wasmValidateText(
        `(module
           (type $f-${type} (func (param ${type}) (result ${type})))
           (func $id-${type} (param ${type}) (result ${type})
             (local.get 0))
           (func $t (result ${type})
             (return_call_indirect (type $f-${type}) (${type}.const ${value})
                                   (i32.const 0)))
           (table anyfunc (elem $id-${type})))`);
}
for (let {type, value} of constants) {
    validateOneArg(type, value);
}

function validateTwoArgs(t0, v0, t1, v1) {
    wasmValidateText(
        `(module
           (type $f (func (param ${t0} ${t1}) (result ${t1})))
           (func $second-${t0}-${t1} (param ${t0} ${t1}) (result ${t1})
             (local.get 1))
           (func $t (result ${t1})
             (return_call_indirect (type $f)
                                   (${t0}.const ${v0}) (${t1}.const ${v1})
                                   (i32.const 0)))
           (table anyfunc (elem $second-${t0}-${t1})))`);
}
for (let {type: t0, value: v0} of constants) {
    for (let {type: t1, value: v1} of constants) {
        validateTwoArgs(t0, v0, t1, v1);
    }
}

wasmValidateText(
    `(module
       (type $f (func (param i64) (result i64)))
       (func $id-i64 (param i64) (result i64)
         (local.get 0))
       (func $dispatch1 (param i32 i64) (result i64)
         (return_call_indirect (type $f) (local.get 1) (local.get 0)))
       (func $dispatch2 (param i32) (result i64)
         (return_call_indirect (type $f) (i64.const 42) (local.get 0)))
       (func $t1 (result i64)
         (return_call $dispatch1 (i32.const 0) (i64.const 1)))
       (func $t2 (result i64)
         (return_call $dispatch2 (i32.const 100)))
       (table anyfunc (elem $id-i64)))`);

wasmValidateText(
    `(module
       (type $i64-i64 (func (param i64 i64) (result i64)))
       (func $fac (type $i64-i64)
         (return_call_indirect (type $i64-i64)
           (local.get 0) (i64.const 1) (i32.const 0)))

       (func $fac-acc (param i64 i64) (result i64)
         (if (result i64) (i64.eqz (local.get 0))
           (then (local.get 1))
           (else
             (return_call_indirect (type $i64-i64)
               (i64.sub (local.get 0) (i64.const 1))
               (i64.mul (local.get 0) (local.get 1))
               (i32.const 0)))))

       (table anyfunc (elem $fac-acc)))`);

wasmValidateText(
    `(module
       (type $t (func (param i32) (result i32)))
       (func $even (param i32) (result i32)
         (if (result i32) (i32.eqz (local.get 0))
           (then (i32.const 44))
           (else
             (return_call_indirect (type $t)
               (i32.sub (local.get 0) (i32.const 1))
               (i32.const 1)))))
       (func $odd (param i32) (result i32)
         (if (result i32) (i32.eqz (local.get 0))
           (then (i32.const 99))
           (else
             (return_call_indirect (type $t)
               (i32.sub (local.get 0) (i32.const 1))
               (i32.const 0)))))
       (table anyfunc (elem $even $odd)))`);

wasmFailValidateText(
    `(module
       (type (func))
       (func $no-table
         (return_call_indirect (type 0) (i32.const 0))))`,
    /call_indirect without a table/);

wasmFailValidateText(
    `(module
       (type (func))
       (table 0 anyfunc)
       (func $type-void-vs-num
         (i32.eqz (return_call_indirect (type 0) (i32.const 0)))))`,
    /unused values not explicitly dropped/);

wasmFailValidateText(
    `(module
       (type (func))
       (table 0 anyfunc)
       (func $type-void-vs-num (result i32)
         (i32.eqz (return_call_indirect (type 0) (i32.const 0)))))`,
    /type mismatch: expected 1 values, got 0 values/);

wasmFailValidateText(
    `(module
       (type (func (result i64)))
       (table 0 anyfunc)
       (func $type-num-vs-num
         (i32.eqz (return_call_indirect (type 0) (i32.const 0)))))`,
    /type mismatch: expected 0 values, got 1 values/);

wasmFailValidateText(
    `(module
       (type (func (result i64)))
       (table 0 anyfunc)
       (func $type-num-vs-num (result i32)
         (i32.eqz (return_call_indirect (type 0) (i32.const 0)))))`,
    /expression has type i64 but expected i32/);

wasmFailValidateText(
    `(module
       (type (func (param i32)))
       (table 0 anyfunc)
       (func $arity-0-vs-1
         (return_call_indirect (type 0) (i32.const 0))))`,
    /popping value from empty stack/);

wasmFailValidateText(
    `(module
       (type (func (param f64 i32)))
       (table 0 anyfunc)
       (func $arity-0-vs-2
         (return_call_indirect (type 0) (i32.const 0))))`,
    /popping value from empty stack/);

wasmFailValidateText(
    `(module
       (type (func (param i32)))
       (table 0 anyfunc)
       (func $type-func-void-vs-i32
         (return_call_indirect (type 0) (i32.const 1) (nop))))`,
    /popping value from empty stack/);

wasmFailValidateText(
    `(module
       (type (func (param i32)))
       (table 0 anyfunc)
       (func $type-func-num-vs-i32
         (return_call_indirect (type 0) (i32.const 0) (i64.const 1))))`,
    /expression has type i64 but expected i32/);

wasmFailValidateText(
    `(module
       (type (func (param i32 i32)))
       (table 0 anyfunc)
       (func $type-first-void-vs-num
         (return_call_indirect (type 0) (nop) (i32.const 1) (i32.const 0))
       ))`,
    /popping value from empty stack/);

wasmFailValidateText(
    `(module
       (type (func (param i32 i32)))
       (table 0 anyfunc)
       (func $type-second-void-vs-num
         (return_call_indirect (type 0) (i32.const 1) (nop) (i32.const 0))))`,
    /popping value from empty stack/);

wasmFailValidateText(
    `(module
       (type (func (param i32 f64)))
       (table 0 anyfunc)
       (func $type-first-num-vs-num
         (return_call_indirect (type 0)
                               (f64.const 1) (i32.const 1) (i32.const 0))))`,
    /expression has type i32 but expected f64/);

wasmFailValidateText(
    `(module
       (type (func (param f64 i32)))
       (table 0 anyfunc)
       (func $type-second-num-vs-num
         (return_call_indirect (type 0)
                               (i32.const 1) (f64.const 1) (i32.const 0))))`,
    /expression has type f64 but expected i32/);

wasmFailValidateText(
    `(module
       (table 0 anyfunc)
       (func $unbound-type
         (return_call_indirect (type 1) (i32.const 0))))`,
    /signature index out of range/);

wasmFailValidateText(
    `(module
       (table 0 anyfunc)
       (func $large-type
         (return_call_indirect (type 1012321300) (i32.const 0))))`,
         /signature index out of range/);
