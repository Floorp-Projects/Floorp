// |jit-test| skip-if: !wasmGcEnabled()

const preamble = `
  (type $s1 (struct))
  (type $s2 (sub $s1 (struct (field i32))))
  (type $a1 (array (ref null $s1)))
  (type $a2 (sub $a1 (array (ref null $s2))))
  (type $f1 (func))

  (func $f (type $f1))
  (elem declare func $f) ;; allow $f to be ref.func'd
`;

const hierarchies = [
  [
    { name: 'any' },
    { name: 'eq' },
    { name: 'struct' },
    { name: '$s1', make: 'struct.new_default $s1' },
    { name: '$s2', make: 'struct.new_default $s2' },
    { name: 'none', none: true },
  ],
  [
    { name: 'any' },
    { name: 'eq' },
    { name: 'array' },
    { name: '$a1', make: '(array.new_default $a1 (i32.const 10))' },
    { name: '$a2', make: '(array.new_default $a2 (i32.const 10))' },
    { name: 'none', none: true },
  ],
  // i31 eventually

  // Apparently we don't support casting functions yet? That should be remedied...
  // [
  //   { name: 'func' },
  //   { name: '$f1', make: 'ref.func $f' },
  //   { name: 'nofunc', none: true },
  // ],

  // TODO: extern
];

const nulls = [           // for example:
  [true, true, true],     // null $s1 -> null any -> null eq
  [true, true, false],    // null $s1 -> null any -> eq
  [false, true, true],    // $s1 -> null any -> null eq
  [false, true, false],   // $s1 -> null any -> eq
  [false, false, true],   // $s1 -> any -> null eq
  [false, false, false],  // $s1 -> any -> eq
]

let numCases = 0;

// Replace this with a string like '$s1 -> any -> any' to test exactly one specific case. Makes debugging easier.
const specificTest = '';

// This will generate an enormous pile of test cases. All of these should be valid,
// as in passing WebAssembly.validate, but some may not be good casts at runtime.
for (const hierarchy of hierarchies) {
  for (const [istart, start] of hierarchy.entries()) {
    for (const [imiddle, middle] of hierarchy.entries()) {
      for (const [iend, end] of hierarchy.entries()) {
        for (const [null0, null1, null2] of nulls) {
          const concreteType = !!start.make;

          if (!concreteType && !null0) {
            // We can only start with null values for abstract types
            continue;
          }

          numCases += 1;

          let good1 = true;
          let good2 = true;

          // Null values will fail casts to non-null types
          if (null0) {
            if (!null1) {
              good1 = false;
            }
            if (!null2) {
              good2 = false;
            }
          }

          // Bottom types can't represent non-null, so this will always fail
          if (!null0) {
            if (middle.none) {
              good1 = false;
            }
            if (end.none) {
              good2 = false;
            }
          }

          // A concrete value cannot be downcast past its actual type
          if (concreteType && !null0) {
            if (istart < imiddle) {
              good1 = false;
            }
            if (istart < iend) {
              good2 = false;
            }
          }

          let emoji1 = good1 ? '✅' : '❌';
          let emoji2 = good2 ? '✅' : '❌';

          if (!good1) {
            good2 = false;
            emoji2 = '❓';
          }

          const name = `${null0 ? 'null ' : ''}${start.name} -> ${null1 ? 'null ' : ''}${middle.name} -> ${null2 ? 'null ' : ''}${end.name}`;
          if (specificTest && name != specificTest) {
            continue;
          }

          print(`${emoji1}${emoji2} ${name}`);
          const make = null0 ? `ref.null ${start.name}` : start.make;
          const type1 = `(ref ${null1 ? 'null ' : ''}${middle.name})`;
          const type2 = `(ref ${null2 ? 'null ' : ''}${end.name})`;
          const moduleText = `(module
            ${preamble}
            (func (export "cast1") (result ${type1})
              ${make}
              ref.cast ${type1}
            )
            (func (export "cast2") (param ${type1}) (result ${type2})
              local.get 0
              ref.cast ${type2}
            )

            (func (export "test1") (result i32)
              ${make}
              ref.test ${type1}
            )
            (func (export "test2") (param ${type1}) (result i32)
              local.get 0
              ref.test ${type2}
            )

            ;; these are basically ref.test but with branches
            (func (export "branch1") (result i32)
              (block (result ${type1})
                ${make}
                br_on_cast 0 anyref ${type1}
                drop
                (return (i32.const 0))
              )
              drop
              (return (i32.const 1))
            )
            (func (export "branch2") (param ${type1}) (result i32)
              (block (result ${type2})
                local.get 0
                br_on_cast 0 anyref ${type2}
                drop
                (return (i32.const 0))
              )
              drop
              (return (i32.const 1))
            )
            (func (export "branchfail1") (result i32)
              (block (result anyref)
                ${make}
                br_on_cast_fail 0 anyref ${type1}
                drop
                (return (i32.const 1))
              )
              drop
              (return (i32.const 0))
            )
            (func (export "branchfail2") (param ${type1}) (result i32)
              (block (result anyref)
                local.get 0
                br_on_cast_fail 0 anyref ${type2}
                drop
                (return (i32.const 1))
              )
              drop
              (return (i32.const 0))
            )
          )`;

          try {
            // The casts are split up so the stack trace will show you which cast is failing.
            const {
              cast1, cast2,
              test1, test2,
              branch1, branch2,
              branchfail1, branchfail2,
            } = wasmEvalText(moduleText).exports;

            function assertCast(func, good) {
              if (good) {
                return [func(), true];
              } else {
                assertErrorMessage(func, WebAssembly.RuntimeError, /bad cast/);
                return [null, false];
              }
            }

            const [res1, ok1] = assertCast(cast1, good1);
            assertEq(test1(), good1 ? 1 : 0);
            assertEq(branch1(), good1 ? 1 : 0);
            assertEq(branchfail1(), good1 ? 1 : 0);
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
        }
      }
    }
  }
}

print(`we hope you have enjoyed these ${numCases} test cases 😁`);
