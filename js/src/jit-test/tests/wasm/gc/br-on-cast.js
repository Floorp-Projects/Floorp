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
      br_on_cast 0 ${castToTypeIndex}
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

// valid: input eqref, output non-nullable struct
validTyping('(type $a (struct))', '$a', ['eqref'], ['(ref $a)']);
// valid: input eqref, output nullable struct
validTyping('(type $a (struct))', '$a', ['eqref'], ['(ref null $a)']);
// valid: input non-nullable struct, output non-nullable struct
validTyping('(type $a (struct)) (type $b (struct))', '$b', ['(ref $a)'], ['(ref $b)']);
// valid: input nullable struct, output non-nullable struct
validTyping('(type $a (struct)) (type $b (struct))', '$b', ['(ref null $a)'], ['(ref $b)']);
// valid: input nullable struct, output nullable struct
validTyping('(type $a (struct)) (type $b (struct))', '$b', ['(ref null $a)'], ['(ref null $b)']);
// valid: input with an extra i32
validTyping('(type $a (struct))', '$a', ['i32', 'eqref'], ['i32', '(ref $a)']);
// valid: input with an extra i32 and f32
validTyping('(type $a (struct))', '$a', ['i32', 'f32', 'eqref'], ['i32', 'f32', '(ref $a)']);

// invalid: block result type must have slot for casted-to type
invalidTyping('(type $a (struct))', '$a', ['eqref'], [], /type mismatch/);
// invalid: block result type must be subtype of casted-to type
invalidTyping('(type $a (struct)) (type $b (struct (field i32)))', '$a', ['eqref'], ['(ref $b)'], /type mismatch/);
// invalid: input is missing extra i32 from the branch target type
invalidTyping('(type $a (struct))', '$a', ['f32', 'eqref'], ['i32', 'f32', '(ref $a)'], /popping value/);
// invalid: input is has extra [i32, f32] swapped from the branch target type
invalidTyping('(type $a (struct))', '$a', ['i32', 'f32', 'eqref'], ['f32', 'i32', '(ref $a)'], /type mismatch/);

// Simple runtime test of casting
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
      (block (result (ref $a))
        local.get 0
        br_on_cast 0 $a

        i32.const 0
        br 1
      )
      drop
      i32.const 1
    )

    (func (export "isB") (param eqref) (result i32)
      (block (result (ref $a))
        local.get 0
        br_on_cast 0 $b

        i32.const 0
        br 1
      )
      drop
      i32.const 1
    )
  )`).exports;

  let a = makeA();
  let b = makeB();

  assertEq(isA(a), 1);
  assertEq(isA(b), 0);
  assertEq(isB(a), 0);
  assertEq(isB(b), 1);
}

// Runtime test of casting with extra values
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

      (func (export "select") (param eqref) (result ${values.map((type) => type).join(" ")})
        (block (result (ref $t))
          local.get 0
          br_on_cast 0 $t

          ${values.map((type, i) => `${type}.const ${values.length + i}`).join("\n")}
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

    assertEqResults(select(t), trueValues);
    assertEqResults(select(f), falseValues);
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
