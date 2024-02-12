/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { ASRouter } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouter.sys.mjs"
);

add_task(async function nested_objects() {
  const target = {
    get foo() {
      return Promise.resolve("foo");
    },
    baz: {
      get qux() {
        return Promise.resolve("qux");
      },
      get corge() {
        return {
          get grault() {
            return Promise.resolve("grault");
          },
        };
      },
    },
  };

  const params = await ASRouter.getTargetingParameters(target);
  Assert.deepEqual(
    params,
    {
      foo: "foo",
      baz: {
        qux: "qux",
        corge: {
          grault: "grault",
        },
      },
    },
    "getTargetingParameters should resolve nested promises"
  );
});

add_task(async function arrays() {
  const target = {
    foo: [1, 2, 3],
    bar: [Promise.resolve(1), Promise.resolve(2), Promise.resolve(3)],
    baz: Promise.resolve([1, 2, 3]),
    qux: Promise.resolve([
      Promise.resolve(1),
      Promise.resolve(2),
      Promise.resolve(3),
    ]),
    quux: Promise.resolve({
      corge: [Promise.resolve(1), 2, 3],
    }),
  };

  const params = await ASRouter.getTargetingParameters(target);
  Assert.deepEqual(
    params,
    {
      foo: [1, 2, 3],
      bar: [1, 2, 3],
      baz: [1, 2, 3],
      qux: [1, 2, 3],
      quux: { corge: [1, 2, 3] },
    },
    "getEnvironmentSnapshot should resolve arrays correctly"
  );
});
