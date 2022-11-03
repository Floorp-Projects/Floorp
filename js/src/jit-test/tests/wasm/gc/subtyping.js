// |jit-test| skip-if: !wasmGcEnabled()

function assertSubtype(superType, subType, types) {
  types = types || [];
  wasmEvalText(`(module
    ${types.map((x, i) => `(type \$${i} ${x})`).join('\n')}
    (func
      unreachable
      (block (param ${subType})
        (block (param ${superType})
          drop
        )
      )
    )
  )`);
}

function assertNotSubtype(superType, subType, types) {
  assertErrorMessage(() => {
    assertSubtype(superType, subType, types);
  }, WebAssembly.CompileError, /type mismatch/);
}

// Primitive trivial subtyping
assertSubtype('i32', 'i32');
assertSubtype('i64', 'i64');
assertSubtype('f32', 'f32');
assertSubtype('f64', 'f64');
assertSubtype('eqref', 'eqref');
assertSubtype('funcref', 'funcref');

// No subtyping relation between funcref, eqref, externref
assertNotSubtype('funcref', 'eqref');
assertNotSubtype('eqref', 'funcref');
assertNotSubtype('funcref', 'externref');
assertNotSubtype('externref', 'funcref');
assertNotSubtype('externref', 'eqref');
assertNotSubtype('eqref', 'externref');

// Structs are subtypes of eqref
assertSubtype(
 'eqref',
 '(ref 0)',
 ['(struct)']);

// Struct identity
assertSubtype(
 '(ref 0)',
 '(ref 1)',
 ['(struct)', '(struct)']);
assertSubtype(
 '(ref 1)',
 '(ref 0)',
 ['(struct)', '(struct)']);

// Self referential struct
assertSubtype(
 '(ref 1)',
 '(ref 0)',
 ['(struct (ref 0))', '(struct (ref 1))']);

// Mutually referential structs
assertSubtype(
 '(ref 2)',
 '(ref 0)',
 ['(struct (ref 1))',
  '(struct (ref 0))',
  '(struct (ref 3))',
  '(struct (ref 2))']);

// Struct subtypes can have extra fields
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct)',
   '(struct (field i32))']);
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct)',
   '(struct (field i32) (field i32))']);

// Struct supertypes cannot have extra fields
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct (field i32))',
   '(struct)']);

// Struct field mutability must match
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct (field (mut i32)))',
   '(struct (field (mut i32)))']);
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct (field i32))',
   '(struct (field i32))']);
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct (field (mut i32)))',
   '(struct (field i32))']);
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct (field i32))',
   '(struct (field (mut i32)))']);

// Struct fields are invariant when mutable
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct (field (mut (ref 2))))',
   '(struct (field (mut (ref 3))))',
   '(struct)',
   '(struct)']);
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct (field (mut (ref 2))))',
   '(struct (field (mut (ref 3))))',
   '(struct)',
   '(struct (field i32))']);

// Struct fields are covariant when immutable
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(struct (field (ref 2)))',
   '(struct (field (ref 3)))',
   '(struct)',
   '(struct (field i32))']);

// Arrays are subtypes of eqref
assertSubtype(
 'eqref',
 '(ref 0)',
 ['(array i32)']);

// Array identity
assertSubtype(
 '(ref 0)',
 '(ref 1)',
 ['(array i32)', '(array i32)']);
assertSubtype(
 '(ref 1)',
 '(ref 0)',
 ['(array i32)', '(array i32)']);

// Self referential array
assertSubtype(
 '(ref 1)',
 '(ref 0)',
 ['(array (ref 0))', '(array (ref 1))']);

// Mutually referential arrays
assertSubtype(
 '(ref 2)',
 '(ref 0)',
 ['(array (ref 1))',
  '(array (ref 0))',
  '(array (ref 3))',
  '(array (ref 2))']);

// Array mutability must match
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(array (mut i32))',
   '(array (mut i32))']);
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(array i32)',
   '(array i32)']);
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(array (mut i32))',
   '(array i32)']);
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(array i32)',
   '(array (mut i32))']);

// Array elements are invariant when mutable
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(array (mut (ref 2)))',
   '(array (mut (ref 3)))',
   '(struct)',
   '(struct)']);
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(array (mut (ref 2)))',
   '(array (mut (ref 3)))',
   '(struct)',
   '(struct (field i32))']);

// Array elements are covariant when immutable
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  ['(array (ref 2))',
   '(array (ref 3))',
   '(struct)',
   '(struct (field i32))']);
