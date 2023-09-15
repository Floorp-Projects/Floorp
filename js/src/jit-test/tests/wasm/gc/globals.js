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
