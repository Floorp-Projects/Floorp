/* eslint-disable @typescript-eslint/require-await */
export const description = `
Unit tests for TestGroup.
`;

import { Fixture } from '../common/framework/fixture.js';
import { makeTestGroup } from '../common/framework/test_group.js';
import { makeTestGroupForUnitTesting } from '../common/internal/test_group.js';
import { assert } from '../common/util/util.js';

import { TestGroupTest } from './test_group_test.js';
import { UnitTest } from './unit_test.js';

export const g = makeTestGroup(TestGroupTest);

g.test('UnitTest_fixture').fn(async t0 => {
  let seen = 0;
  function count(t: Fixture): void {
    seen++;
  }

  const g = makeTestGroupForUnitTesting(UnitTest);

  g.test('test').fn(count);
  g.test('testp')
    .paramsSimple([{ a: 1 }])
    .fn(count);

  await t0.run(g);
  t0.expect(seen === 2);
});

g.test('custom_fixture').fn(async t0 => {
  let seen = 0;
  class Counter extends UnitTest {
    count(): void {
      seen++;
    }
  }

  const g = makeTestGroupForUnitTesting(Counter);

  g.test('test').fn(t => {
    t.count();
  });
  g.test('testp')
    .paramsSimple([{ a: 1 }])
    .fn(t => {
      t.count();
    });

  await t0.run(g);
  t0.expect(seen === 2);
});

g.test('stack').fn(async t0 => {
  const g = makeTestGroupForUnitTesting(UnitTest);

  const doNestedThrow1 = () => {
    throw new Error('goodbye');
  };

  const doNestedThrow2 = () => doNestedThrow1();

  g.test('fail').fn(t => {
    t.fail();
  });
  g.test('throw').fn(t => {
    throw new Error('hello');
  });
  g.test('throw_nested').fn(t => {
    doNestedThrow2();
  });

  const res = await t0.run(g);

  const search = /unittests[/\\]test_group\.spec\.[tj]s/;
  t0.expect(res.size > 0);
  for (const { logs } of res.values()) {
    assert(logs !== undefined, 'expected logs');
    t0.expect(logs.some(l => search.test(l.toJSON())));
    t0.expect(search.test(logs[logs.length - 1].toJSON()));
  }
});

g.test('no_fn').fn(t => {
  const g = makeTestGroupForUnitTesting(UnitTest);

  g.test('missing');

  t.shouldThrow('Error', () => {
    g.validate();
  });
});

g.test('duplicate_test_name').fn(t => {
  const g = makeTestGroupForUnitTesting(UnitTest);
  g.test('abc').fn(() => {});

  t.shouldThrow('Error', () => {
    g.test('abc').fn(() => {});
  });
});

g.test('duplicate_test_params,none').fn(() => {
  {
    const g = makeTestGroupForUnitTesting(UnitTest);
    g.test('abc')
      .paramsSimple([])
      .fn(() => {});
    g.validate();
  }

  {
    const g = makeTestGroupForUnitTesting(UnitTest);
    g.test('abc').fn(() => {});
    g.validate();
  }

  {
    const g = makeTestGroupForUnitTesting(UnitTest);
    g.test('abc')
      .paramsSimple([
        { a: 1 }, //
      ])
      .fn(() => {});
    g.validate();
  }
});

g.test('duplicate_test_params,basic').fn(t => {
  {
    const g = makeTestGroupForUnitTesting(UnitTest);
    const builder = g.test('abc');
    t.shouldThrow('Error', () => {
      builder.paramsSimple([
        { a: 1 }, //
        { a: 1 },
      ]);
      g.validate();
    });
  }
  {
    const g = makeTestGroupForUnitTesting(UnitTest);
    g.test('abc')
      .params(u =>
        u.expandWithParams(() => [
          { a: 1 }, //
          { a: 1 },
        ])
      )
      .fn(() => {});
    t.shouldThrow('Error', () => {
      g.validate();
    });
  }
  {
    const g = makeTestGroupForUnitTesting(UnitTest);
    g.test('abc')
      .paramsSimple([
        { a: 1, b: 3 }, //
        { b: 3, a: 1 },
      ])
      .fn(() => {});
    t.shouldThrow('Error', () => {
      g.validate();
    });
  }
});

g.test('duplicate_test_params,with_different_private_params').fn(t => {
  {
    const g = makeTestGroupForUnitTesting(UnitTest);
    const builder = g.test('abc');
    t.shouldThrow('Error', () => {
      builder.paramsSimple([
        { a: 1, _b: 1 }, //
        { a: 1, _b: 2 },
      ]);
    });
  }
  {
    const g = makeTestGroupForUnitTesting(UnitTest);
    g.test('abc')
      .params(u =>
        u.expandWithParams(() => [
          { a: 1, _b: 1 }, //
          { a: 1, _b: 2 },
        ])
      )
      .fn(() => {});
    t.shouldThrow('Error', () => {
      g.validate();
    });
  }
});

g.test('invalid_test_name').fn(t => {
  const g = makeTestGroupForUnitTesting(UnitTest);

  const badChars = Array.from('"`~@#$+=\\|!^&*[]<>{}-\'. ');
  for (const char of badChars) {
    const name = 'a' + char + 'b';
    t.shouldThrow(
      'Error',
      () => {
        g.test(name).fn(() => {});
      },
      name
    );
  }
});

g.test('param_value,valid').fn(() => {
  const g = makeTestGroup(UnitTest);
  g.test('a').paramsSimple([{ x: JSON.stringify({ a: 1, b: 2 }) }]);
});

g.test('param_value,invalid').fn(t => {
  for (const badChar of ';=*') {
    const g = makeTestGroupForUnitTesting(UnitTest);
    const builder = g.test('a');
    t.shouldThrow('Error', () => {
      builder.paramsSimple([{ badChar }]);
    });
  }
});

g.test('subcases').fn(async t0 => {
  const g = makeTestGroupForUnitTesting(UnitTest);
  g.test('a')
    .paramsSubcasesOnly(u =>
      u //
        .combineWithParams([{ a: 1 }])
    )
    .fn(t => {
      t.expect(t.params.a === 1, 'a must be 1');
    });

  function* gen({ a, b }: { a?: number; b?: number }) {
    if (b === 2) {
      yield { ret: 2 };
    } else if (a === 1) {
      yield { ret: 1 };
    } else {
      yield { ret: -1 };
    }
  }
  g.test('b')
    .params(u =>
      u
        .combineWithParams([{ a: 1 }, { b: 2 }])
        .beginSubcases()
        .expandWithParams(gen)
    )
    .fn(t => {
      const { a, b, ret } = t.params;
      t.expect((a === 1 && ret === 1) || (b === 2 && ret === 2));
    });

  const result = await t0.run(g);
  t0.expect(Array.from(result.values()).every(v => v.status === 'pass'));
});

g.test('exceptions')
  .params(u =>
    u
      .combine('useSubcases', [false, true]) //
      .combine('useDOMException', [false, true])
  )
  .fn(async t0 => {
    const { useSubcases, useDOMException } = t0.params;
    const g = makeTestGroupForUnitTesting(UnitTest);

    const b1 = g.test('a');
    let b2;
    if (useSubcases) {
      b2 = b1.paramsSubcasesOnly(u => u);
    } else {
      b2 = b1.params(u => u);
    }
    b2.fn(t => {
      if (useDOMException) {
        throw new DOMException('Message!', 'Name!');
      } else {
        throw new Error('Message!');
      }
    });

    const result = await t0.run(g);
    const values = Array.from(result.values());
    t0.expect(values.length === 1);
    t0.expect(values[0].status === 'fail');
  });

g.test('throws').fn(async t0 => {
  const g = makeTestGroupForUnitTesting(UnitTest);

  g.test('a').fn(t => {
    throw new Error();
  });

  const result = await t0.run(g);
  const values = Array.from(result.values());
  t0.expect(values.length === 1);
  t0.expect(values[0].status === 'fail');
});

g.test('shouldThrow').fn(async t0 => {
  t0.shouldThrow('TypeError', () => {
    throw new TypeError();
  });

  const g = makeTestGroupForUnitTesting(UnitTest);

  g.test('a').fn(t => {
    t.shouldThrow('Error', () => {
      throw new TypeError();
    });
  });

  const result = await t0.run(g);
  const values = Array.from(result.values());
  t0.expect(values.length === 1);
  t0.expect(values[0].status === 'fail');
});

g.test('shouldReject').fn(async t0 => {
  t0.shouldReject(
    'TypeError',
    (async () => {
      throw new TypeError();
    })()
  );

  const g = makeTestGroupForUnitTesting(UnitTest);

  g.test('a').fn(t => {
    t.shouldReject(
      'Error',
      (async () => {
        throw new TypeError();
      })()
    );
  });

  const result = await t0.run(g);
  // Fails even though shouldReject doesn't fail until after the test function ends
  const values = Array.from(result.values());
  t0.expect(values.length === 1);
  t0.expect(values[0].status === 'fail');
});
