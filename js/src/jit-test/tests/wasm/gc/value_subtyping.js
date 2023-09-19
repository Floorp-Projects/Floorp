// |jit-test| skip-if: !wasmGcEnabled()

function simpleTypeSection(types) {
  return types.map((x, i) => `(type \$${i} ${x})`).join('\n');
}

function assertSubtype(superType, subType, types) {
  types = types || [];
  wasmEvalText(`(module
    ${types}
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
assertSubtype('i31ref', 'i31ref');
assertSubtype('funcref', 'funcref');

// No subtyping relation between funcref, anyref, externref. These are our top
// types.
assertNotSubtype('funcref', 'anyref');
assertNotSubtype('anyref', 'funcref');
assertNotSubtype('funcref', 'externref');
assertNotSubtype('externref', 'funcref');
assertNotSubtype('externref', 'anyref');
assertNotSubtype('anyref', 'externref');

// eqref is a subtype of anyref
assertSubtype('anyref', 'eqref');

// i31ref is a subtype of eqref
assertSubtype('anyref', 'i31ref');
assertSubtype('eqref', 'i31ref');

// structref is a subtype of eqref and anyref
assertSubtype('anyref', 'structref');
assertSubtype('eqref', 'structref');

// arrayref is a subtype of eqref and anyref
assertSubtype('anyref', 'arrayref');
assertSubtype('eqref', 'arrayref');

// Structs are subtypes of anyref, eqref, and structref
assertSubtype(
 'anyref',
 '(ref 0)',
 simpleTypeSection(['(struct)']));
assertSubtype(
 'eqref',
 '(ref 0)',
 simpleTypeSection(['(struct)']));
assertSubtype(
 'structref',
 '(ref 0)',
 simpleTypeSection(['(struct)']));

// Struct identity
assertSubtype(
 '(ref 0)',
 '(ref 1)',
 simpleTypeSection(['(struct)', '(struct)']));
assertSubtype(
 '(ref 1)',
 '(ref 0)',
 simpleTypeSection(['(struct)', '(struct)']));

// Self referential struct
assertSubtype(
 '(ref 1)',
 '(ref 0)',
 simpleTypeSection(['(struct (field (ref 0)))', '(struct (field (ref 1)))']));

// Mutually referential structs
assertSubtype(
 '(ref 2)',
 '(ref 0)',
 `(rec
    (type (struct (field (ref 1))))
    (type (struct (field (ref 0))))
  )
  (rec
    (type (struct (field (ref 3))))
    (type (struct (field (ref 2))))
  )`);

// Struct subtypes can have extra fields
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  `(type (sub (struct)))
   (type (sub 0 (struct (field i32))))`);
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  `(type (sub (struct)))
   (type (sub 0 (struct (field i32) (field i32))))`);

// Struct supertypes cannot have extra fields
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  simpleTypeSection([
    '(struct (field i32))',
    '(struct)']));

// Struct field mutability must match
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  simpleTypeSection([
    '(struct (field (mut i32)))',
    '(struct (field (mut i32)))']));
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  simpleTypeSection([
    '(struct (field i32))',
    '(struct (field i32))']));
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  simpleTypeSection([
    '(struct (field (mut i32)))',
    '(struct (field i32))']));
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  simpleTypeSection([
    '(struct (field i32))',
    '(struct (field (mut i32)))']));

// Struct fields are invariant when mutable
assertSubtype(
  '(ref 2)',
  '(ref 3)',
  simpleTypeSection([
    '(struct)',
    '(struct)',
    '(struct (field (mut (ref 0))))',
    '(struct (field (mut (ref 1))))']));
assertNotSubtype(
  '(ref 2)',
  '(ref 3)',
  simpleTypeSection([
    '(struct)',
    '(struct (field i32))',
    '(struct (field (mut (ref 0))))',
    '(struct (field (mut (ref 1))))']));

// Struct fields are covariant when immutable
assertSubtype(
  '(ref 2)',
  '(ref 3)',
  `(type (sub (struct)))
   (type (sub 0 (struct (field i32))))
   (type (sub (struct (field (ref 0)))))
   (type (sub 2 (struct (field (ref 1)))))`);

// Arrays are subtypes of anyref, eqref, and arrayref
assertSubtype(
 'anyref',
 '(ref 0)',
 simpleTypeSection(['(array i32)']));
assertSubtype(
 'eqref',
 '(ref 0)',
 simpleTypeSection(['(array i32)']));
assertSubtype(
 'arrayref',
 '(ref 0)',
 simpleTypeSection(['(array i32)']));

// Array identity
assertSubtype(
 '(ref 0)',
 '(ref 1)',
 simpleTypeSection(['(array i32)', '(array i32)']));
assertSubtype(
 '(ref 1)',
 '(ref 0)',
 simpleTypeSection(['(array i32)', '(array i32)']));

// Self referential array
assertSubtype(
 '(ref 1)',
 '(ref 0)',
 simpleTypeSection(['(array (ref 0))', '(array (ref 1))']));

// Mutually referential arrays
assertSubtype(
 '(ref 2)',
 '(ref 0)',
 `(rec
   (type (array (ref 1)))
   (type (array (ref 0)))
  )
  (rec
   (type (array (ref 3)))
   (type (array (ref 2)))
  )`);

// Array mutability must match
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  simpleTypeSection([
    '(array (mut i32))',
    '(array (mut i32))']));
assertSubtype(
  '(ref 0)',
  '(ref 1)',
  simpleTypeSection([
    '(array i32)',
    '(array i32)']));
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  simpleTypeSection([
    '(array (mut i32))',
    '(array i32)']));
assertNotSubtype(
  '(ref 0)',
  '(ref 1)',
  simpleTypeSection([
    '(array i32)',
    '(array (mut i32))']));

// Array elements are invariant when mutable
assertSubtype(
  '(ref 2)',
  '(ref 3)',
  simpleTypeSection([
   '(struct)',
   '(struct)',
   '(array (mut (ref 0)))',
   '(array (mut (ref 1)))']));
assertNotSubtype(
  '(ref 2)',
  '(ref 3)',
  simpleTypeSection([
   '(struct)',
   '(struct (field i32))',
   '(array (mut (ref 0)))',
   '(array (mut (ref 1)))']));

// Array elements are covariant when immutable
assertSubtype(
  '(ref 2)',
  '(ref 3)',
  simpleTypeSection([
  '(sub (struct))',
  '(sub 0 (struct (field i32)))',
  '(sub (array (ref 0)))',
  '(sub 2 (array (ref 1)))']));

// nullref is a subtype of everything in anyref hierarchy
assertSubtype('anyref', 'nullref');
assertSubtype('eqref', 'nullref');
assertSubtype('structref', 'nullref');
assertSubtype('arrayref', 'nullref');
assertSubtype('(ref null 0)', 'nullref', simpleTypeSection(['(struct)']));
assertSubtype('(ref null 0)', 'nullref', simpleTypeSection(['(array i32)']));

// nullref is not a subtype of any other hierarchy
assertNotSubtype('funcref', 'nullref');
assertNotSubtype('(ref null 0)', 'nullref', simpleTypeSection(['(func)']));
assertNotSubtype('externref', 'nullref');

// nullfuncref is a subtype of everything in funcref hierarchy
assertSubtype('funcref', 'nullfuncref');
assertSubtype('(ref null 0)', 'nullfuncref', simpleTypeSection(['(func)']));

// nullfuncref is not a subtype of any other hierarchy
assertNotSubtype('anyref', 'nullfuncref');
assertNotSubtype('eqref', 'nullfuncref');
assertNotSubtype('structref', 'nullfuncref');
assertNotSubtype('arrayref', 'nullfuncref');
assertNotSubtype('externref', 'nullfuncref');
assertNotSubtype('(ref null 0)', 'nullfuncref', simpleTypeSection(['(struct)']));
assertNotSubtype('(ref null 0)', 'nullfuncref', simpleTypeSection(['(array i32)']));

// nullexternref is a subtype of everything in externref hierarchy
assertSubtype('externref', 'nullexternref');

// nullexternref is not a subtype of any other hierarchy
assertNotSubtype('anyref', 'nullexternref');
assertNotSubtype('eqref', 'nullexternref');
assertNotSubtype('structref', 'nullexternref');
assertNotSubtype('arrayref', 'nullexternref');
assertNotSubtype('funcref', 'nullexternref');
assertNotSubtype('(ref null 0)', 'nullexternref', simpleTypeSection(['(struct)']));
assertNotSubtype('(ref null 0)', 'nullexternref', simpleTypeSection(['(array i32)']));
assertNotSubtype('(ref null 0)', 'nullexternref', simpleTypeSection(['(func)']));
