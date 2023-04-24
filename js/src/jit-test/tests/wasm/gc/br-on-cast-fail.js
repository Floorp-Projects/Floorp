// |jit-test| skip-if: !wasmGcEnabled()

function typingModule(types, from, to, brParams, branchResults, fallthroughResults) {
  return `(module
    ${types}
    (func
      (param ${brParams.join(' ')})
      (result ${branchResults.join(' ')})

      (block (result ${fallthroughResults.join(' ')})
        (; push params onto the stack in the same order as they appear, leaving
          the last param at the top of the stack. ;)
        ${brParams.map((_, i) => `local.get ${i}`).join('\n')}
        br_on_cast_fail 1 ${from} ${to}
      )
      unreachable
    )
  )`;
}

function validTyping(types, from, to, brParams, branchResults, fallthroughResults) {
  wasmValidateText(typingModule(types, from, to, brParams, branchResults, fallthroughResults));
}

function invalidTyping(types, from, to, brParams, branchResults, fallthroughResults, error) {
  wasmFailValidateText(typingModule(types, from, to, brParams, branchResults, fallthroughResults), error);
}

// valid: eqref -> struct
validTyping('(type $a (struct))', 'eqref', '(ref $a)', ['eqref'], ['eqref'], ['(ref $a)']);
// valid: eqref -> struct (and looser types on results)
validTyping('(type $a (struct))', 'eqref', '(ref $a)', ['eqref'], ['anyref'], ['(ref null $a)']);
// valid: eqref -> nullable struct (note that branch type becomes non-nullable)
validTyping('(type $a (struct))', 'eqref', '(ref null $a)', ['eqref'], ['(ref eq)'], ['(ref null $a)']);
// valid: struct -> struct (from anyref)
validTyping('(type $a (struct))', 'anyref', '(ref $a)', ['(ref $a)'], ['anyref'], ['(ref $a)']);
// valid: struct -> struct (canonicalized)
validTyping('(type $a (struct)) (type $b (struct))', '(ref $a)', '(ref $b)', ['(ref $a)'], ['(ref $b)'], ['(ref $a)']);
// valid: nullable struct -> non-nullable struct (canonicalized)
validTyping('(type $a (struct)) (type $b (struct))', '(ref null $a)', '(ref $b)', ['(ref null $a)'], ['(ref null $a)'], ['(ref $b)']);
// valid: nullable struct -> nullable struct (canonicalized)
validTyping('(type $a (struct)) (type $b (struct))', '(ref null $a)', '(ref null $b)', ['(ref null $a)'], ['(ref $a)'], ['(ref null $a)']);
// valid: eqref -> struct with extra arg
validTyping('(type $a (struct))', 'eqref', '(ref $a)', ['i32', 'eqref'], ['i32', 'eqref'], ['i32', '(ref $a)']);
// valid: eqref -> struct with two extra args
validTyping('(type $a (struct))', 'eqref', '(ref $a)', ['i32', 'f32', 'eqref'], ['i32', 'f32', 'eqref'], ['i32', 'f32', '(ref $a)']);

// invalid: block result type must have slot for casted-to type
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['eqref'], [], ['(ref $a)'], /type mismatch/);
// invalid: block result type must be supertype of casted-to type
invalidTyping('(type $a (struct)) (type $b (struct (field i32)))', 'eqref', '(ref $a)', ['eqref'], ['(ref $b)'], ['(ref $a)'], /type mismatch/);
// invalid: input is missing extra i32 from the branch target type
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['f32', 'eqref'], ['i32', 'f32', 'eqref'], ['i32', 'f32', '(ref $a)'], /popping value/);
// invalid: input has extra [i32, f32] swapped from the branch target type
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['i32', 'f32', 'eqref'], ['f32', 'i32', 'eqref'], ['i32', 'f32', '(ref $a)'], /type mismatch/);
// invalid: input has extra [i32, f32] swapped from the branch fallthrough type
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['i32', 'f32', 'eqref'], ['i32', 'f32', 'eqref'], ['f32', 'i32', '(ref $a)'], /type mismatch/);
// invalid: casting to non-nullable but fallthrough not nullable
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['eqref'], ['(ref $a)'], ['(ref eq)'], /type mismatch/);
// invalid: struct -> struct (same recursion group)
invalidTyping('(rec (type $a (struct)) (type $b (struct)))', '(ref $a)', '(ref $b)', ['(ref $a)'], ['(ref $a)'], ['(ref $b)'], /type mismatch/);

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
        br_on_cast_fail 0 eqref (ref $a)

        i32.const 1
        br 1
      )
      drop
      i32.const 0
    )

    (func (export "isB") (param eqref) (result i32)
      (block (result eqref)
        local.get 0
        br_on_cast_fail 0 eqref (ref $b)

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
          br_on_cast_fail 0 eqref (ref $t)

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
         br_on_cast_fail 0 eqref (ref $a)
         unreachable
       )
     )`;
    let { onCastFail } = wasmEvalText(tOnCastFail).exports;
}
