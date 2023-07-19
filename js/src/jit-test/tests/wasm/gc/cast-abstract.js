// |jit-test| skip-if: !wasmGcEnabled()

load(libdir + "wasm-binary.js");

// Replace this with a string like 'non-null (ref $s1) -> (ref any) -> (ref any)' to test exactly one specific case. Makes debugging easier.
const specificTest = '';

const preamble = `
  (type $s1 (sub (struct)))
  (type $s2 (sub $s1 (struct (field i32))))
  (type $a1 (sub (array (ref null $s1))))
  (type $a2 (sub $a1 (array (ref null $s2))))
  (type $ft1 (sub (func (param (ref $s1)) (result (ref null $a1)))))
  (type $ft2 (sub $ft1 (func (param (ref null $s1)) (result (ref null $a2)))))
  (type $ft3 (sub (func (param (ref $a2)) (result i32))))
  (type $ft4 (sub $ft3 (func (param (ref $a1)) (result i32))))

  (func $f1 (type $ft1) ref.null $a1)
  (func $f2 (type $ft2) ref.null $a2)
  (func $f3 (type $ft3) i32.const 0)
  (func $f4 (type $ft4) i32.const 0)
  (elem declare func $f1 $f2 $f3 $f4) ;; allow funcs to be ref.func'd
`;
const preambleTypesV5 = [
  { kind: StructCode, final: false, fields: [] },
  { kind: StructCode, final: false, sub: 0, fields: [I32Code] },
  { kind: ArrayCode, final: false, elem: [OptRefCode, ...varU32(0)] },
  { kind: ArrayCode, final: false, sub: 2, elem: [OptRefCode, ...varU32(1)] },
]

// All the concrete subtype relationships present in the preamble.
// [x, y] means x <: y.
const subtypes = [
  ['$s2', '$s1'],
  ['$a2', '$a1'],
  ['$ft2', '$ft1'],
  ['$ft4', '$ft3'],
];

const any = { name: 'any', top: 'anyref' };
const eq = { name: 'eq', top: 'anyref' };
const struct = { name: 'struct', top: 'anyref' };
const array = { name: 'array', top: 'anyref' };
const s1 = {
  index: 0,
  name: '$s1',
  make: 'struct.new_default $s1',
  makeV5: [0xfb, 0x08, ...varU32(0)], // struct.new_default 0
  top: 'anyref',
};
const s2 = {
  index: 1,
  name: '$s2',
  make: 'struct.new_default $s2',
  makeV5: [0xfb, 0x08, ...varU32(1)], // struct.new_default 1
  top: 'anyref',
};
const a1 = {
  index: 2,
  name: '$a1',
  make: '(array.new_default $a1 (i32.const 10))',
  makeV5: [
    I32ConstCode, ...varS32(10), // i32.const 10
    0xfb, 0x1c, ...varU32(2), // array.new_default 2
  ],
  top: 'anyref',
};
const a2 = {
  index: 3,
  name: '$a2',
  make: '(array.new_default $a2 (i32.const 10))',
  makeV5: [
    I32ConstCode, ...varS32(10), // i32.const 10
    0xfb, 0x1c, ...varU32(3), // array.new_default 3
  ],
  top: 'anyref',
};
const none = { name: 'none', none: true, top: 'anyref' };

const func = { name: 'func', top: 'funcref' };
const ft1 = { index: 4, name: '$ft1', make: 'ref.func $f1', top: 'funcref' };
const ft2 = { index: 5, name: '$ft2', make: 'ref.func $f2', top: 'funcref' };
const ft3 = { index: 6, name: '$ft3', make: 'ref.func $f3', top: 'funcref' };
const ft4 = { index: 7, name: '$ft4', make: 'ref.func $f4', top: 'funcref' };
const nofunc = { name: 'nofunc', none: true, top: 'funcref' };

const extern = { name: 'extern', make: '(extern.externalize (struct.new_default $s1))', top: 'externref' }
const noextern = { name: 'noextern', none: true, top: 'externref' }

const typeSets = [
  [any, eq, struct, s1, s2, none],
  [any, eq, array, a1, a2, none],
  [any, eq, s1, s2, a1, a2, none],
  // i31 eventually

  [func, ft1, ft2, nofunc],
  [func, ft3, ft4, nofunc],
  [func, ft1, ft2, ft3, ft4, nofunc],

  [extern, noextern],
];

const nullables = [       // for example:
  [true, true, true],     // null $s1 -> null any -> null eq
  [true, true, false],    // null $s1 -> null any -> eq
  [true, false, true],    // null $s1 -> any -> null eq
  [true, false, false],   // null $s1 -> any -> eq
  [false, true, true],    // $s1 -> null any -> null eq
  [false, true, false],   // $s1 -> null any -> eq
  [false, false, true],   // $s1 -> any -> null eq
  [false, false, false],  // $s1 -> any -> eq
]

function isSubtype(src, dest) {
  if (src.name === dest.name) {
    return true;
  }
  for (const [src2, dest2] of subtypes) {
    if (src.name === src2 && dest.name === dest2) {
      return true;
    }
  }
  return false;
}

let numCases = 0;

// This will generate an enormous pile of test cases. All of these should be valid,
// as in passing WebAssembly.validate, but some may not be good casts at runtime.
for (const typeSet of typeSets) {
  for (const start of typeSet) {
    for (const middle of typeSet) {
      for (const end of typeSet) {
        for (const [nullable0, nullable1, nullable2] of nullables) {
          for (const makeNull of [true, false]) {
            const concrete0 = !!start.make;
            const concrete1 = !!middle.make;
            const concrete2 = !!end.make;

            if (!concrete0 && !makeNull) {
              // We can only start with null values for abstract types
              continue;
            }

            if (!nullable0 && makeNull) {
              // Can't use null as a valid value for a non-nullable type
              continue;
            }

            numCases += 1;

            let good1 = true;
            let good2 = true;

            // Null values will fail casts to non-nullable types
            if (makeNull) {
              if (!nullable1) {
                good1 = false;
              }
              if (!nullable2) {
                good2 = false;
              }
            }

            // Bottom types can't represent non-null, so this will always fail
            if (!makeNull) {
              if (middle.none) {
                good1 = false;
              }
              if (end.none) {
                good2 = false;
              }
            }

            // Concrete values are subject to subtyping relationships
            if (!makeNull) {
              if (concrete1 && !isSubtype(start, middle)) {
                good1 = false;
              }
              if (concrete2 && !isSubtype(start, end)) {
                good2 = false;
              }
            }

            // Because the encoding is wildly different, we only test concrete types in V5
            let doV5 = concrete0 && concrete1 && concrete2;
            if ((nullable0 && !nullable1) || (nullable1 && !nullable2)) {
              // also v5 ref.test can't do null tests
              doV5 = false;
            }
            if (start.top !== 'anyref' || middle.top !== 'anyref' || end.top !== 'anyref') {
              // v5 instructions only support gc types
              doV5 = false;
            }

            let emoji1 = good1 ? '✅' : '❌';
            let emoji2 = good2 ? '✅' : '❌';

            if (!good1) {
              good2 = false;
              emoji2 = '❓';
            }

            const name = `${makeNull ? 'null' : 'non-null'} (ref ${nullable0 ? 'null ' : ''}${start.name}) -> (ref ${nullable1 ? 'null ' : ''}${middle.name}) -> (ref ${nullable2 ? 'null ' : ''}${end.name})`;
            if (specificTest && name != specificTest) {
              continue;
            }

            print(`${emoji1}${emoji2} ${name}`);
            const make = makeNull ? `ref.null ${start.name}` : start.make;
            const type0 = `(ref ${nullable0 ? 'null ' : ''}${start.name})`;
            const type1 = `(ref ${nullable1 ? 'null ' : ''}${middle.name})`;
            const type2 = `(ref ${nullable2 ? 'null ' : ''}${end.name})`;
            const moduleText = `(module
              ${preamble}
              (func (export "make") (result ${type0})
                ${make}
              )
              (func (export "cast1") (param ${type0}) (result ${type1})
                local.get 0
                ref.cast ${type1}
              )
              (func (export "cast2") (param ${type1}) (result ${type2})
                local.get 0
                ref.cast ${type2}
              )

              (func (export "test1") (param ${type0}) (result i32)
                local.get 0
                ref.test ${type1}
              )
              (func (export "test2") (param ${type1}) (result i32)
                local.get 0
                ref.test ${type2}
              )

              ;; these are basically ref.test but with branches
              (func (export "branch1") (param ${type0}) (result i32)
                (block (result ${type1})
                  local.get 0
                  br_on_cast 0 ${start.top} ${type1}
                  drop
                  (return (i32.const 0))
                )
                drop
                (return (i32.const 1))
              )
              (func (export "branch2") (param ${type1}) (result i32)
                (block (result ${type2})
                  local.get 0
                  br_on_cast 0 ${middle.top} ${type2}
                  drop
                  (return (i32.const 0))
                )
                drop
                (return (i32.const 1))
              )
              (func (export "branchfail1") (param ${type0}) (result i32)
                (block (result ${middle.top})
                  local.get 0
                  br_on_cast_fail 0 ${start.top} ${type1}
                  drop
                  (return (i32.const 1))
                )
                drop
                (return (i32.const 0))
              )
              (func (export "branchfail2") (param ${type1}) (result i32)
                (block (result ${end.top})
                  local.get 0
                  br_on_cast_fail 0 ${middle.top} ${type2}
                  drop
                  (return (i32.const 1))
                )
                drop
                (return (i32.const 0))
              )
            )`;

            // Now we recreate the above module with the V5 instruction encodings.
            // I look forward to the day when we can delete this :)
            let v5ModuleBytes;
            if (doV5) {
              const makeV5 = makeNull ? [RefNullCode, ...varS32(start.index)] : start.makeV5;
              const type1v5 = [nullable1 ? OptRefCode : RefCode, ...varS32(middle.index)];
              const type2v5 = [nullable2 ? OptRefCode : RefCode, ...varS32(end.index)];
              v5ModuleBytes = moduleWithSections([
                typeSection([
                  ...preambleTypesV5,
                  /* 4 */ { kind: FuncCode, args: [], ret: [type1v5] },
                  /* 5 */ { kind: FuncCode, args: [type1v5], ret: [type2v5] },
                  /* 6 */ { kind: FuncCode, args: [], ret: [I32Code] },
                  /* 7 */ { kind: FuncCode, args: [type1v5], ret: [I32Code] },
                ]),
                declSection([4, 5, 6, 7]),
                exportSection([
                  { funcIndex: 0, name: "cast1V5" },
                  { funcIndex: 1, name: "cast2V5" },
                  { funcIndex: 2, name: "test1V5" },
                  { funcIndex: 3, name: "test2V5" },
                ]),
                bodySection([
                  // cast1V5
                  funcBody({ locals: [], body: [
                    ...makeV5,
                    0xfb, 0x45, ...varU32(middle.index) // ref.cast.v5 $middle
                  ] }),
                  // cast2V5
                  funcBody({ locals: [], body: [
                    LocalGetCode, ...varU32(0), // local.get 0
                    0xfb, 0x45, ...varU32(end.index) // ref.cast.v5 $end
                  ] }),
                  // test1V5
                  funcBody({ locals: [], body: [
                    ...makeV5,
                    0xfb, 0x44, ...varU32(middle.index) // ref.test.v5 $middle
                  ] }),
                  // test2V5
                  funcBody({ locals: [], body: [
                    LocalGetCode, ...varU32(0), // local.get 0
                    0xfb, 0x44, ...varU32(end.index) // ref.test.v5 $end
                  ] }),
                ]),
              ]);
            }

            function assertCast(func, good) {
              if (good) {
                return [func(), true];
              } else {
                assertErrorMessage(func, WebAssembly.RuntimeError, /bad cast/);
                return [null, false];
              }
            }

            try {
              // The casts are split up so the stack trace will show you which cast is failing.
              const {
                make,
                cast1, cast2,
                test1, test2,
                branch1, branch2,
                branchfail1, branchfail2,
              } = wasmEvalText(moduleText).exports;

              const val = make();
              const [res1, ok1] = assertCast(() => cast1(val), good1);
              assertEq(test1(val), good1 ? 1 : 0);
              assertEq(branch1(val), good1 ? 1 : 0);
              assertEq(branchfail1(val), good1 ? 1 : 0);
              if (ok1) {
                assertCast(() => cast2(res1), good2);
                assertEq(test2(res1), good2 ? 1 : 0);
                assertEq(branch2(res1), good2 ? 1 : 0);
                assertEq(branchfail2(res1), good2 ? 1 : 0);
              }
            } catch (e) {
              print("Failed! Module text was:");
              print(moduleText);
              throw e;
            }

            // and again for v5...
            if (doV5) {
              try {
                const {
                  cast1V5, cast2V5,
                  test1V5, test2V5,
                } = new WebAssembly.Instance(new WebAssembly.Module(v5ModuleBytes)).exports;

                // Differences from the above:
                // - ref.test.v5 always returns 0 for null
                // - ref.cast.v5 can't cast from nullable to non-nullable

                const [res1, ok1] = assertCast(cast1V5, good1);
                assertEq(test1V5(), (good1 && !makeNull) ? 1 : 0);
                if (ok1) {
                  assertCast(() => cast2V5(res1), good2);
                  assertEq(test2V5(res1), (good2 && !makeNull) ? 1 : 0);
                }
              } catch (e) {
                print("Failed v5 encoding test! Sadly we have no module text for you, but here are the bytes:");
                print(v5ModuleBytes);
                throw e;
              }
            }
          }
        }
      }
    }
  }
}

print(`we hope you have enjoyed these ${numCases} test cases 😁`);
