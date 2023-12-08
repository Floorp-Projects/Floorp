// |jit-test| skip-if: !wasmGcEnabled()

// Globals have identity now
{
  const { g, same } = wasmEvalText(`(module
    (type (struct))
    (global (export "g") eqref (struct.new 0))
    (func $same (export "same") (param eqref) (result i32)
      (ref.eq (local.get 0) (global.get 0))
    )
    (func $sanity
      (if (call $same (global.get 0)) (then return))
      unreachable
    )
    (start $sanity)
  )`).exports;

  assertEq(same(g.value), 1, "global had different identity when exported");
}

// Subtypes with func refs
{
  wasmEvalText(`(module
    (type $s1 (struct))
    (type $t1 (sub (func (param (ref $s1)))))
    (type $t2 (sub $t1 (func (param (ref null $s1)))))
    (func $a (type $t1))
    (func $b (type $t2))

    (global (ref $t1) ref.func $a)
    (global (ref null $t1) ref.func $a)
    (global (ref func) ref.func $a)
    (global (ref null func) ref.func $a)

    (global (ref $t2) ref.func $b)
    (global (ref null $t2) ref.func $b)
    (global (ref $t1) ref.func $b)
    (global (ref null $t1) ref.func $b)
    (global (ref func) ref.func $b)
    (global (ref null func) ref.func $b)
  )`);

  assertErrorMessage(() => wasmEvalText(`(module
    (type $s1 (struct))
    (type $t1 (func (param (ref $s1))))
    (type $t2 (func (param (ref null $s1)))) ;; not a subtype of t1
    (func $a (type $t2))
    (global (ref $t1) ref.func $a)
  )`), WebAssembly.CompileError, /type mismatch/);
}

// Subtypes with struct refs
{
  wasmEvalText(`(module
    (type $t1 (sub (struct)))
    (type $t2 (sub $t1 (struct (field i32))))

    (global (ref $t1) struct.new_default $t1)
    (global (ref null $t1) struct.new_default $t1)
    (global (ref struct) struct.new_default $t1)
    (global (ref null struct) struct.new_default $t1)
    (global (ref eq) struct.new_default $t1)
    (global (ref null eq) struct.new_default $t1)
    (global (ref any) struct.new_default $t1)
    (global (ref null any) struct.new_default $t1)

    (global (ref $t2) struct.new_default $t2)
    (global (ref null $t2) struct.new_default $t2)
    (global (ref $t1) struct.new_default $t2)
    (global (ref null $t1) struct.new_default $t2)
    (global (ref struct) struct.new_default $t2)
    (global (ref null struct) struct.new_default $t2)
    (global (ref eq) struct.new_default $t2)
    (global (ref null eq) struct.new_default $t2)
    (global (ref any) struct.new_default $t2)
    (global (ref null any) struct.new_default $t2)
  )`);

  assertErrorMessage(() => wasmEvalText(`(module
    (type $t1 (struct))
    (type $t2 (struct (field i32))) ;; not a subtype of t1
    (global (ref $t1) struct.new_default $t2)
  )`), WebAssembly.CompileError, /type mismatch/);
}

// Subtypes with array refs
{
  wasmEvalText(`(module
    (type $s (struct))
    (type $t1 (sub (array (ref null $s))))
    (type $t2 (sub $t1 (array (ref $s))))

    (global (ref $t1) array.new_fixed $t1 0)
    (global (ref null $t1) array.new_fixed $t1 0)
    (global (ref array) array.new_fixed $t1 0)
    (global (ref null array) array.new_fixed $t1 0)
    (global (ref eq) array.new_fixed $t1 0)
    (global (ref null eq) array.new_fixed $t1 0)
    (global (ref any) array.new_fixed $t1 0)
    (global (ref null any) array.new_fixed $t1 0)

    (global (ref $t2) array.new_fixed $t2 0)
    (global (ref null $t2) array.new_fixed $t2 0)
    (global (ref $t1) array.new_fixed $t2 0)
    (global (ref null $t1) array.new_fixed $t2 0)
    (global (ref array) array.new_fixed $t2 0)
    (global (ref null array) array.new_fixed $t2 0)
    (global (ref eq) array.new_fixed $t2 0)
    (global (ref null eq) array.new_fixed $t2 0)
    (global (ref any) array.new_fixed $t2 0)
    (global (ref null any) array.new_fixed $t2 0)
  )`);

  assertErrorMessage(() => wasmEvalText(`(module
    (type $s (struct))
    (type $t1 (array (ref null $s)))
    (type $t2 (array (ref $s))) ;; not a subtype of t1
    (global (ref $t1) array.new_fixed $t2 0)
  )`), WebAssembly.CompileError, /type mismatch/);
}

// Subtypes should be respected on imports and exports
{
  const { struct, mut_struct, eq, mut_eq, any, mut_any } = wasmEvalText(`(module
    (type (struct))
    (global (export "struct") structref (struct.new 0))
    (global (export "mut_struct") (mut structref) (struct.new 0))
    (global (export "eq") eqref (struct.new 0))
    (global (export "mut_eq") (mut eqref) (struct.new 0))
    (global (export "any") anyref (struct.new 0))
    (global (export "mut_any") (mut anyref) (struct.new 0))
  )`).exports;

  function importGlobalIntoType(g, t) {
    wasmEvalText(`(module
      (global (import "test" "g") ${t})
    )`, { "test": { "g": g } });
  }

  importGlobalIntoType(struct, `structref`);
  importGlobalIntoType(struct, `eqref`);
  importGlobalIntoType(struct, `anyref`);

  importGlobalIntoType(mut_struct, `(mut structref)`);
  assertErrorMessage(() => importGlobalIntoType(mut_struct, `(mut eqref)`), WebAssembly.LinkError, /global type mismatch/);
  assertErrorMessage(() => importGlobalIntoType(mut_struct, `(mut anyref)`), WebAssembly.LinkError, /global type mismatch/);

  assertErrorMessage(() => importGlobalIntoType(eq, `structref`), WebAssembly.LinkError, /global type mismatch/);
  importGlobalIntoType(eq, `eqref`);
  importGlobalIntoType(eq, `anyref`);

  assertErrorMessage(() => importGlobalIntoType(mut_eq, `(mut structref)`), WebAssembly.LinkError, /global type mismatch/);
  importGlobalIntoType(mut_eq, `(mut eqref)`);
  assertErrorMessage(() => importGlobalIntoType(mut_eq, `(mut anyref)`), WebAssembly.LinkError, /global type mismatch/);

  assertErrorMessage(() => importGlobalIntoType(any, `structref`), WebAssembly.LinkError, /global type mismatch/);
  assertErrorMessage(() => importGlobalIntoType(any, `eqref`), WebAssembly.LinkError, /global type mismatch/);
  importGlobalIntoType(any, `anyref`);

  assertErrorMessage(() => importGlobalIntoType(mut_any, `(mut structref)`), WebAssembly.LinkError, /global type mismatch/);
  assertErrorMessage(() => importGlobalIntoType(mut_any, `(mut eqref)`), WebAssembly.LinkError, /global type mismatch/);
  importGlobalIntoType(mut_any, `(mut anyref)`);
}

// Importing globals with ref types
{
  const { struct, array, func } = wasmEvalText(`(module
    (type (struct))
    (type (array i32))
    (func $f)
    (global (export "struct") structref (struct.new 0))
    (global (export "array") arrayref (array.new_fixed 1 0))
    (global (export "func") funcref (ref.func $f))
  )`).exports;

  function importValueIntoType(v, t) {
    wasmEvalText(`(module
      (global (import "test" "v") ${t})
    )`, { "test": { "v": v } });
  }

  assertErrorMessage(() => importValueIntoType(struct.value, `(mut structref)`), WebAssembly.LinkError, /global mutability mismatch/);

  importValueIntoType(null, `structref`);
  assertErrorMessage(() => importValueIntoType(null, `(ref struct)`), TypeError, /cannot pass null/);
  assertErrorMessage(() => importValueIntoType(0, `structref`), TypeError, /can only pass/);
  importValueIntoType(struct.value, `structref`);
  assertErrorMessage(() => importValueIntoType(array.value, `structref`), TypeError, /can only pass/);

  importValueIntoType(null, `i31ref`);
  assertErrorMessage(() => importValueIntoType(null, `(ref i31)`), TypeError, /cannot pass null/);
  importValueIntoType(0, `i31ref`);
  assertErrorMessage(() => importValueIntoType(0.1, `i31ref`), TypeError, /can only pass/);
  assertErrorMessage(() => importValueIntoType("test", `i31ref`), TypeError, /can only pass/);
  assertErrorMessage(() => importValueIntoType(struct.value, `i31ref`), TypeError, /can only pass/);

  importValueIntoType(null, `eqref`);
  assertErrorMessage(() => importValueIntoType(null, `(ref eq)`), TypeError, /cannot pass null/);
  assertErrorMessage(() => importValueIntoType(undefined, `(ref eq)`), TypeError, /can only pass/);
  importValueIntoType(0, `eqref`);
  assertErrorMessage(() => importValueIntoType(0.1, `eqref`), TypeError, /can only pass/);
  assertErrorMessage(() => importValueIntoType((x)=>x, `eqref`), TypeError, /can only pass/);
  assertErrorMessage(() => importValueIntoType("test", `eqref`), TypeError, /can only pass/);
  importValueIntoType(struct.value, `eqref`);
  assertErrorMessage(() => importValueIntoType(func.value, `eqref`), TypeError, /can only pass/);

  importValueIntoType(null, `anyref`);
  assertErrorMessage(() => importValueIntoType(null, `(ref any)`), TypeError, /cannot pass null/);
  importValueIntoType(undefined, `(ref any)`);
  importValueIntoType(0, `anyref`);
  importValueIntoType(0.1, `anyref`);
  importValueIntoType((x)=>x, `anyref`)
  importValueIntoType("test", `anyref`);
  importValueIntoType(struct.value, `anyref`);
  importValueIntoType(func.value, `anyref`);

  importValueIntoType(null, `externref`);
  assertErrorMessage(() => importValueIntoType(null, `(ref extern)`), TypeError, /cannot pass null/);
  importValueIntoType(undefined, `(ref extern)`);
  importValueIntoType(0, `externref`);
  importValueIntoType(0.1, `externref`);
  importValueIntoType((x)=>x, `externref`)
  importValueIntoType("test", `externref`);
  importValueIntoType(struct.value, `externref`);
  importValueIntoType(func.value, `externref`);

  importValueIntoType(null, `funcref`);
  assertErrorMessage(() => importValueIntoType(null, `(ref func)`), TypeError, /cannot pass null/);
  assertErrorMessage(() => importValueIntoType(0, `funcref`), TypeError, /can only pass/);
  assertErrorMessage(() => importValueIntoType((x)=>x, `funcref`), TypeError, /can only pass/);
  importValueIntoType(func.value, `funcref`)
  importValueIntoType(func.value, `(ref func)`)
}
