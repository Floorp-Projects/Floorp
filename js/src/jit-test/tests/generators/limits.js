// |jit-test| skip-if: getBuildConfiguration("wasi")
//
// Tests aimed at AbstractGeneratorObject::FixedSlotLimit.

"use strict";

function test(n) {
  const iterate = (start, f) => {
    let value = start;
    for (let i = n; i-- > 0; ) {
      value = f(value, i);
    }
    return value;
  };

  const generate = (start, f) => {
    let s = iterate(start, f);
    let gen = eval('(function* () {\n' + s + '})');
    return gen();
  };

  // Test 1: many vars in the function scope
  {
    let it = generate(
      "yield 0;",
      (s, i) => `
        var v${i} = ${i};
        ${s}
        assertEq(v${i}, ${i});
      `
    );
    assertEq(it.next().done, false);
    assertEq(it.next().done, true);
  }

  // Test 2: many let-bindings in nested lexical scopes
  {
    let it = generate(
      "yield a => v174;",
      (s, i) => {
        let block = `
          let v${i} = ${i};
          ${s}
          assertEq(v${i}, ${i});
        `;
        if (i % 17 == 0) {
          block = '{\n' + block + '}\n';
        }
        return block;
      }
    );
    assertEq(it.next().done, false);
    assertEq(it.next().done, true);
  }

  // Test 3: TDZ is preserved across yield
  {
    let it = generate(
      'yield 0;\n' +
        'try { v1; } catch (exc) { yield [1, exc]; }\n' +
        `try { v${n - 1}; } catch (exc) { yield [2, exc]; }\n`,
      (s, i) => {
        let block = `
          ${s}
          let v${i};
        `;
        if (i % 256 == 0) {
          block = '{\n' + block + '}\n';
        }
        return block;
      }
    );
    let {value, done} = it.next();
    assertEq(value, 0);
    ({value, done} = it.next());
    assertEq(value[0], 1);
    assertEq(value[1].name, "ReferenceError");
    ({value, done} = it.next());
    assertEq(value[0], 2);
    assertEq(value[1].name, "ReferenceError");
    ({value, done} = it.next());
    assertEq(done, true);
  }
}

for (let exp = 8; exp < 12; exp++) {
  const n = 2 ** exp;
  test(n - 2);
  test(n + 1);
}
