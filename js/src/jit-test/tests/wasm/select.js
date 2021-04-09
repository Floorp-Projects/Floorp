// Test the select instruction

wasmFailValidateText('(module (func (select (i32.const 0) (i32.const 0) (f32.const 0))))', mismatchError("f32", "i32"));

wasmFailValidateText('(module (func (select (i32.const 0) (f32.const 0) (i32.const 0))) (export "" (func 0)))', /(select operand types must match)|(type mismatch: expected f32, found i32)/);
wasmFailValidateText('(module (func (select (block ) (i32.const 0) (i32.const 0))) (export "" (func 0)))', emptyStackError);
wasmFailValidateText('(module (func (select (return) (i32.const 0) (i32.const 0))) (export "" (func 0)))', unusedValuesError);
assertEq(wasmEvalText('(module (func (drop (select (return) (i32.const 0) (i32.const 0)))) (export "" (func 0)))').exports[""](), undefined);
assertEq(wasmEvalText('(module (func (result i32) (i32.add (i32.const 0) (select (return (i32.const 42)) (i32.const 0) (i32.const 0)))) (export "" (func 0)))').exports[""](), 42);
wasmFailValidateText('(module (func (select (if (result i32) (i32.const 1) (i32.const 0) (f32.const 0)) (i32.const 0) (i32.const 0))) (export "" (func 0)))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func) (func (select (call 0) (call 0) (i32.const 0))) (export "" (func 0)))', emptyStackError);

(function testSideEffects() {

var numT = 0;
var numF = 0;

var imports = {"": {
    ifTrue: () => 1 + numT++,
    ifFalse: () => -1 + numF++,
}}

// Test that side-effects are applied on both branches.
var f = wasmEvalText(`
(module
 (import "" "ifTrue" (func (result i32)))
 (import "" "ifFalse" (func (result i32)))
 (func (param i32) (result i32)
  (select
   (call 0)
   (call 1)
   (local.get 0)
  )
 )
 (export "" (func 2))
)
`, imports).exports[""];

assertEq(f(-1), numT);
assertEq(numT, 1);
assertEq(numF, 1);

assertEq(f(0), numF - 2);
assertEq(numT, 2);
assertEq(numF, 2);

assertEq(f(1), numT);
assertEq(numT, 3);
assertEq(numF, 3);

assertEq(f(0), numF - 2);
assertEq(numT, 4);
assertEq(numF, 4);

assertEq(f(0), numF - 2);
assertEq(numT, 5);
assertEq(numF, 5);

assertEq(f(1), numT);
assertEq(numT, 6);
assertEq(numF, 6);

})();

function testNumSelect(type, trueVal, falseVal) {
    var trueJS = jsify(trueVal);
    var falseJS = jsify(falseVal);

    // Always true condition
    var alwaysTrue = wasmEvalText(`
    (module
     (func (param i32) (result ${type})
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (i32.const 1)
      )
     )
     (export "" (func 0))
    )
    `, {}).exports[""];

    assertEq(alwaysTrue(0), trueJS);
    assertEq(alwaysTrue(1), trueJS);
    assertEq(alwaysTrue(-1), trueJS);

    // Always false condition
    var alwaysFalse = wasmEvalText(`
    (module
     (func (param i32) (result ${type})
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (i32.const 0)
      )
     )
     (export "" (func 0))
    )
    `, {}).exports[""];

    assertEq(alwaysFalse(0), falseJS);
    assertEq(alwaysFalse(1), falseJS);
    assertEq(alwaysFalse(-1), falseJS);

    // Variable condition
    var f = wasmEvalText(`
    (module
     (func (param i32) (result ${type})
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (local.get 0)
      )
     )
     (export "" (func 0))
    )
    `, {}).exports[""];

    assertEq(f(0), falseJS);
    assertEq(f(1), trueJS);
    assertEq(f(-1), trueJS);

    wasmFullPass(`
    (module
     (func (param i32) (result ${type})
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (local.get 0)
      )
     )
     (export "run" (func 0))
    )`,
    trueJS,
    {},
    1);
}

testNumSelect('i32', 13, 37);
testNumSelect('i32', Math.pow(2, 31) - 1, -Math.pow(2, 31));

testNumSelect('f32', Math.fround(13.37), Math.fround(19.89));
testNumSelect('f32', 'inf', '-0');
testNumSelect('f32', 'nan', Math.pow(2, -31));

testNumSelect('f64', 13.37, 19.89);
testNumSelect('f64', 'inf', '-0');
testNumSelect('f64', 'nan', Math.pow(2, -31));

wasmAssert(`
(module
 (func $f (param i32) (result i64)
  (select
   (i64.const 0xc0010ff08badf00d)
   (i64.const 0x12345678deadc0de)
   (local.get 0)
  )
 )
 (export "" (func 0))
)`, [
    { type: 'i64', func: '$f', args: ['i32.const  0'], expected: '0x12345678deadc0de' },
    { type: 'i64', func: '$f', args: ['i32.const  1'], expected: '0xc0010ff08badf00d' },
    { type: 'i64', func: '$f', args: ['i32.const -1'], expected: '0xc0010ff08badf00d' },
], {});

wasmFailValidateText(`(module
    (func (param externref)
      (select (local.get 0) (local.get 0) (i32.const 0))
      drop
    ))`,
    /(untyped select)|(type mismatch: select only takes integral types)/);

wasmFailValidateText(`(module
    (func (param funcref)
      (select (local.get 0) (local.get 0) (i32.const 0))
      drop
    ))`,
    /(untyped select)|(type mismatch: select only takes integral types)/);

function testRefSelect(type, trueVal, falseVal) {
    // Always true condition
    var alwaysTrue = wasmEvalText(`
    (module
     (func (param i32 ${type} ${type}) (result ${type})
      (select (result ${type})
       local.get 1
       local.get 2
       (i32.const 1)
      )
     )
     (export "" (func 0))
    )
    `, {}).exports[""];

    assertEq(alwaysTrue(0, trueVal, falseVal), trueVal);
    assertEq(alwaysTrue(1, trueVal, falseVal), trueVal);
    assertEq(alwaysTrue(-1, trueVal, falseVal), trueVal);

    // Always false condition
    var alwaysFalse = wasmEvalText(`
    (module
     (func (param i32 ${type} ${type}) (result ${type})
      (select (result ${type})
       local.get 1
       local.get 2
       (i32.const 0)
      )
     )
     (export "" (func 0))
    )
    `, {}).exports[""];

    assertEq(alwaysFalse(0, trueVal, falseVal), falseVal);
    assertEq(alwaysFalse(1, trueVal, falseVal), falseVal);
    assertEq(alwaysFalse(-1, trueVal, falseVal), falseVal);

    // Variable condition
    var f = wasmEvalText(`
    (module
     (func (param i32 ${type} ${type}) (result ${type})
      (select (result ${type})
        local.get 1
        local.get 2
       (local.get 0)
      )
     )
     (export "" (func 0))
    )
    `, {}).exports[""];

    assertEq(f(0, trueVal, falseVal), falseVal);
    assertEq(f(1, trueVal, falseVal), trueVal);
    assertEq(f(-1, trueVal, falseVal), trueVal);

    wasmFullPass(`
    (module
     (func (param i32 ${type} ${type}) (result ${type})
      (select (result ${type})
       local.get 1
       local.get 2
       (local.get 0)
      )
     )
     (export "run" (func 0))
    )`,
    trueVal,
    {},
    1,
    trueVal,
    falseVal);
}

testRefSelect('externref', {}, {});

let {export1, export2} = wasmEvalText(`(module
    (func (export "export1"))
    (func (export "export2"))
)`).exports;

testRefSelect('funcref', export1, export2);
