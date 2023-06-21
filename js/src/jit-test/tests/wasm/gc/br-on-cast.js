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
        br_on_cast 1 ${from} ${to}
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
validTyping('(type $a (struct))', 'eqref', '(ref $a)', ['eqref'], ['(ref $a)'], ['eqref']);
// valid: eqref -> struct (and looser types on results)
validTyping('(type $a (struct))', 'eqref', '(ref $a)', ['eqref'], ['(ref null $a)'], ['anyref']);
// valid: eqref -> nullable struct (note that fallthrough becomes non-nullable)
validTyping('(type $a (struct))', 'eqref', '(ref null $a)', ['eqref'], ['(ref null $a)'], ['(ref eq)']);
// valid: struct -> struct (from anyref)
validTyping('(type $a (struct))', 'anyref', '(ref $a)', ['(ref $a)'], ['(ref $a)'], ['anyref']);
// valid: struct -> struct (canonicalized)
validTyping('(type $a (struct)) (type $b (struct))', '(ref $a)', '(ref $b)', ['(ref $a)'], ['(ref $b)'], ['(ref $b)']);
// valid: nullable struct -> non-nullable struct (canonicalized)
validTyping('(type $a (struct)) (type $b (struct))', '(ref null $a)', '(ref $b)', ['(ref null $a)'], ['(ref $b)'], ['(ref null $a)']);
// valid: nullable struct -> nullable struct (canonicalized)
validTyping('(type $a (struct)) (type $b (struct))', '(ref null $a)', '(ref null $b)', ['(ref null $a)'], ['(ref null $a)'], ['(ref $a)']);
// valid: eqref -> struct with extra arg
validTyping('(type $a (struct))', 'eqref', '(ref $a)', ['i32', 'eqref'], ['i32', '(ref $a)'], ['i32', 'eqref']);
// valid: eqref -> struct with two extra args
validTyping('(type $a (struct))', 'eqref', '(ref $a)', ['i32', 'f32', 'eqref'], ['i32', 'f32', '(ref $a)'], ['i32', 'f32', 'eqref']);

// invalid: block result type must have slot for casted-to type
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['eqref'], [], ['eqref'], /type mismatch/);
// invalid: block result type must be supertype of casted-to type
invalidTyping('(type $a (struct)) (type $b (struct (field i32)))', 'eqref', '(ref $a)', ['eqref'], ['(ref $b)'], ['(ref $a)'], /type mismatch/);
// invalid: input is missing extra i32 from the branch target type
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['f32', 'eqref'], ['i32', 'f32', '(ref $a)'], ['i32', 'f32', 'eqref'], /popping value/);
// invalid: input has extra [i32, f32] swapped from the branch target type
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['i32', 'f32', 'eqref'], ['f32', 'i32', '(ref $a)'], ['i32', 'f32', 'eqref'], /type mismatch/);
// invalid: input has extra [i32, f32] swapped from the branch fallthrough type
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['i32', 'f32', 'eqref'], ['i32', 'f32', '(ref $a)'], ['f32', 'i32', 'eqref'], /type mismatch/);
// invalid: casting to non-nullable but fallthrough not nullable
invalidTyping('(type $a (struct))', 'eqref', '(ref $a)', ['eqref'], ['(ref $a)'], ['(ref eq)'], /type mismatch/);
// invalid: struct -> struct (same recursion group)
invalidTyping('(rec (type $a (struct)) (type $b (struct)))', '(ref $a)', '(ref $b)', ['(ref $a)'], ['(ref $b)'], ['(ref $a)'], /type mismatch/);

// Simple runtime test of casting
{
  let { makeA, makeB, isA, isB } = wasmEvalText(`(module
    (type $a (sub (struct)))
    (type $b (sub $a (struct (field i32))))

    (func (export "makeA") (result eqref)
      struct.new_default $a
    )

    (func (export "makeB") (result eqref)
      struct.new_default $b
    )

    (func (export "isA") (param eqref) (result i32)
      (block (result (ref $a))
        local.get 0
        br_on_cast 0 anyref (ref $a)

        i32.const 0
        br 1
      )
      drop
      i32.const 1
    )

    (func (export "isB") (param eqref) (result i32)
      (block (result (ref $a))
        local.get 0
        br_on_cast 0 anyref (ref $b)

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
  assertEq(isA(b), 1);
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
          br_on_cast 0 anyref (ref $t)

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

// This test causes the `values` vector returned by
// `OpIter<Policy>::readBrOnCast` to contain three entries, the last of which
// is the argument, hence is reftyped.  This is used to verify an assertion to
// that effect in FunctionCompiler::brOnCastCommon.
{
    let tOnCast =
    `(module
       (type $a (struct))
       (func (export "onCast") (param f32 i32 eqref) (result f32 i32 (ref $a))
         local.get 0
         local.get 1
         local.get 2
         br_on_cast 0 anyref (ref $a)
         unreachable
       )
     )`;
    let { onCast } = wasmEvalText(tOnCast).exports;
}
