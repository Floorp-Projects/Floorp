// |jit-test| skip-if: !wasmGcEnabled()

// Test all possible casting combinations of the following graph:
//
//  A1       A2
//  |        |
//  B1       B2
//  | \      |
//  C1  C2   C3
//  | \      |
//  D1  D2   D3
//  | \      |
//  E1  E2   E3
//  | \      |
//  F1  F2   F3
//  | \      |
//  G1  G2   G3
//  | \      |
//  H1  H2   H3
//  | \      |
//  I1  I2   I3
//  | \      |
//  J1  J2   J3
//
// NOTE: this object will be mutated and needs to be ordered such that parent
// definitions come before children.  Note also, to be properly effective,
// these trees need to have a depth of at least MinSuperTypeVectorLength as
// defined in wasm/WasmCodegenConstants.h; keep it in sync with that.
const TYPES = {
  'A1': { super: null },
  'A2': { super: null },
  'B1': { super: 'A1' },
  'B2': { super: 'A2' },
  'C1': { super: 'B1' },
  'C2': { super: 'B1' },
  'C3': { super: 'B2' },
  'D1': { super: 'C1' },
  'D2': { super: 'C1' },
  'D3': { super: 'C3' },
  'E1': { super: 'D1' },
  'E2': { super: 'D1' },
  'E3': { super: 'D3' },
  'F1': { super: 'E1' },
  'F2': { super: 'E1' },
  'F3': { super: 'E3' },
  'G1': { super: 'F1' },
  'G2': { super: 'F1' },
  'G3': { super: 'F3' },
  'H1': { super: 'G1' },
  'H2': { super: 'G1' },
  'H3': { super: 'G3' },
  'I1': { super: 'H1' },
  'I2': { super: 'H1' },
  'I3': { super: 'H3' },
  'J1': { super: 'I1' },
  'J2': { super: 'I1' },
  'J3': { super: 'I3' },
};

// The oracle method for testing the declared subtype relationship.
function manualIsSubtype(types, subType, superType) {
  while (subType !== superType && subType.super !== null) {
    subType = types[subType.super];
  }
  return subType === superType;
}

function testAllCasts(types) {
  let typeSection = ``;
  let funcSection = ``;
  for (let name in types) {
    let type = types[name];
    if (type.super === null) {
      typeSection += `(type \$${name} (sub (struct)))\n`;
    } else {
      typeSection += `(type \$${name} (sub \$${type.super} (struct)))\n`;
    }
    funcSection += `
      (func (export "new${name}") (result externref)
        struct.new_default \$${name}
        extern.convert_any
      )
      (func (export "is${name}") (param externref) (result i32)
        local.get 0
        any.convert_extern
        ref.test (ref \$${name})
      )`;
  }
  // NOTE: we place all types in a single recursion group to prevent
  // canonicalization from 
  let moduleText = `(module
    (rec ${typeSection})
    ${funcSection}
  )`;

  // Instantiate the module and acquire the testing methods
  let exports = wasmEvalText(moduleText).exports;
  for (let name in types) {
    let type = types[name];
    type['new'] = exports[`new${name}`];
    type['is'] = exports[`is${name}`];
  }

  // Test every combination of types, comparing the oracle method against the
  // JIT'ed method.
  for (let subTypeName in types) {
    let subType = types[subTypeName];
    for (let superTypeName in types) {
      let superType = types[subTypeName];
      assertEq(
        manualIsSubtype(types, subType, superType) ? 1 : 0,
        superType['is'](subType['new']()));
    }
  }
}
testAllCasts(TYPES);
