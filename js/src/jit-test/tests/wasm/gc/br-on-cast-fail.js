// |jit-test| skip-if: !wasmGcEnabled()

function typingModule(types, castToTypeIndex, brParams, blockResults) {
  return `(module
    ${types}
    (func
      (param ${brParams.join(' ')})
      (result ${blockResults.join(' ')})

      (; push params onto the stack in the same order as they appear, leaving
         the last param at the top of the stack. ;)
      ${brParams.map((_, i) => `local.get ${i}`).join('\n')}
      br_on_cast_fail 0 ${castToTypeIndex}
      unreachable
    )
  )`;
}

function validTyping(types, castToTypeIndex, brParams, blockResults) {
  wasmValidateText(typingModule(types, castToTypeIndex, brParams, blockResults));
}

function invalidTyping(types, castToTypeIndex, brParams, blockResults, error) {
  wasmFailValidateText(typingModule(types, castToTypeIndex, brParams, blockResults), error);
}

// valid: input non-nullable struct, output non-nullable struct
validTyping('(type $a (struct)) (type $b (struct))', '$b', ['(ref $a)'],
            ['(ref $b)']);
// valid: input nullable struct, output nullable struct
validTyping('(type $a (struct)) (type $b (struct))', '$b', ['(ref null $a)'],
            ['(ref null $b)']);
// valid: input with an extra i32
validTyping('(type $a (struct))', '$a', ['i32', 'eqref'], ['i32', 'eqref']);
// valid: input with an extra i32 and f32
validTyping('(type $a (struct))', '$a', ['i32', 'f32', 'eqref'],
            ['i32', 'f32', 'eqref']);

// invalid: input eqref, output non-nullable struct
invalidTyping('(type $a (struct))', '$a', ['eqref'], ['(ref $a)'],
              /type mismatch/);
// invalid: input eqref, output nullable struct
invalidTyping('(type $a (struct))', '$a', ['eqref'], ['(ref null $a)'],
              /type mismatch/);
// invalid: input nullable struct, output non-nullable struct
invalidTyping('(type $a (struct)) (type $b (struct))', '$b', ['(ref null $a)'],
              ['(ref $b)'],/type mismatch/);
// invalid: block result type must have slot for casted-to type
invalidTyping('(type $a (struct))', '$a', ['eqref'], [], /type mismatch/);
// invalid: block result type must be subtype of casted-to type
invalidTyping('(type $a (struct)) (type $b (struct (field i32)))', '$a',
              ['eqref'], ['(ref $b)'], /type mismatch/);
// invalid: input is missing extra i32 from the branch target type
invalidTyping('(type $a (struct))', '$a', ['f32', 'eqref'],
              ['i32', 'f32', 'eqref'], /popping value/);
// invalid: input is has extra [i32, f32] swapped from the branch target type
invalidTyping('(type $a (struct))', '$a', ['i32', 'f32', 'eqref'],
              ['f32', 'i32', '(ref $a)'], /type mismatch/);

// Simple runtime test of cast-fail-ing
{
  let { makeA, makeB, isA, isB } = wasmEvalText(`(module
    (type $a (struct))
    (type $b (struct (field i32)))

    (func (export "makeA") (result eqref)
      struct.new_default $a
    )

    (func (export "makeB") (result eqref)
      struct.new_default $b
    )

    (func (export "isA") (param eqref) (result i32)
      (block (result eqref)
        local.get 0
        br_on_cast_fail 0 $a

        i32.const 1
        br 1
      )
      drop
      i32.const 0
    )

    (func (export "isB") (param eqref) (result i32)
      (block (result eqref)
        local.get 0
        br_on_cast_fail 0 $b

        i32.const 1
        br 1
      )
      drop
      i32.const 0
    )
  )`).exports;

  let a = makeA();
  let b = makeB();

  assertEq(isA(a), 1);
  assertEq(isA(b), 0);
  assertEq(isB(a), 0);
  assertEq(isB(b), 1);
}

// Runtime test of cast-fail-ing with extra values
{
  function assertEqResults(a, b) {
    if (!(a instanceof Array)) {
      a = [a];
    }
    if (!(b instanceof Array)) {
      b = [b];
    }
    if (a.length !== b.length) {
      assertEq(a.length, b.length);
    }
    for (let i = 0; i < a.length; i++) {
      let x = a[i];
      let y = b[i];
      // intentionally use loose equality to allow bigint to compare equally
      // to number, as can happen with how we use the JS-API here.
      assertEq(x == y, true);
    }
  }

  function testExtra(values) {
    let { makeT, makeF, select } = wasmEvalText(`(module
      (type $t (struct))
      (type $f (struct (field i32)))

      (func (export "makeT") (result eqref)
        struct.new_default $t
      )
      (func (export "makeF") (result eqref)
        struct.new_default $f
      )

      (func (export "select")
            (param eqref) (result ${values.map((type) => type).join(" ")})
        (block (result eqref)
          local.get 0
          br_on_cast_fail 0 $t

          ${values.map((type, i) => `${type}.const ${values.length + i}`)
                  .join("\n")}
          br 1
        )
        drop
        ${values.map((type, i) => `${type}.const ${i}`).join("\n")}
      )
    )`).exports;

    let t = makeT();
    let f = makeF();

    let trueValues = values.map((type, i) => i);
    let falseValues = values.map((type, i) => values.length + i);

    assertEqResults(select(t), falseValues);
    assertEqResults(select(f), trueValues);
  }

  // multiples of primitive valtypes
  for (let valtype of ['i32', 'i64', 'f32', 'f64']) {
    testExtra([valtype]);
    testExtra([valtype, valtype]);
    testExtra([valtype, valtype, valtype]);
    testExtra([valtype, valtype, valtype, valtype, valtype, valtype, valtype, valtype]);
  }

  // random sundry of valtypes
  testExtra(['i32', 'f32', 'i64', 'f64']);
  testExtra(['i32', 'f32', 'i64', 'f64', 'i32', 'f32', 'i64', 'f64']);
}

// This test causes the `values` vector returned by
// `OpIter<Policy>::readBrOnCastFail` to contain three entries, the last of
// which is the argument, hence is reftyped.  This is used to verify an
// assertion to that effect in FunctionCompiler::brOnCastCommon.
{
    let tOnCastFail =
    `(module
       (type $a (struct))
       (func (export "onCastFail") (param f32 i32 eqref) (result f32 i32 eqref)
         local.get 0
         local.get 1
         local.get 2
         br_on_cast_fail 0 $a
         unreachable
       )
     )`;
    let { onCastFail } = wasmEvalText(tOnCastFail).exports;
}
