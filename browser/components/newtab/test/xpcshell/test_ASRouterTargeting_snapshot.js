/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { ASRouterTargeting } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);

add_task(async function should_ignore_rejections() {
  let target = {
    get foo() {
      return new Promise(resolve => resolve(1));
    },

    get bar() {
      return new Promise((resolve, reject) => reject(new Error("unspecified")));
    },
  };

  let snapshot = await ASRouterTargeting.getEnvironmentSnapshot(target);
  Assert.deepEqual(snapshot, { environment: { foo: 1 }, version: 1 });
});

add_task(async function nested_objects() {
  const target = {
    get foo() {
      return Promise.resolve("foo");
    },
    get bar() {
      return Promise.reject(new Error("bar"));
    },
    baz: {
      get qux() {
        return Promise.resolve("qux");
      },
      get quux() {
        return Promise.reject(new Error("quux"));
      },
      get corge() {
        return {
          get grault() {
            return Promise.resolve("grault");
          },
          get garply() {
            return Promise.reject(new Error("garply"));
          },
        };
      },
    },
  };

  const snapshot = await ASRouterTargeting.getEnvironmentSnapshot(target);
  Assert.deepEqual(
    snapshot,
    {
      environment: {
        foo: "foo",
        baz: {
          qux: "qux",
          corge: {
            grault: "grault",
          },
        },
      },
      version: 1,
    },
    "getEnvironmentSnapshot should resolve nested promises"
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

  const snapshot = await ASRouterTargeting.getEnvironmentSnapshot(target);
  Assert.deepEqual(
    snapshot,
    {
      environment: {
        foo: [1, 2, 3],
        bar: [1, 2, 3],
        baz: [1, 2, 3],
        qux: [1, 2, 3],
        quux: { corge: [1, 2, 3] },
      },
      version: 1,
    },
    "getEnvironmentSnapshot should resolve arrays correctly"
  );
});

/*
 * NB: This test is last because it manipulates shutdown phases.
 *
 * Adding tests after this one will result in failures.
 */
add_task(async function should_ignore_rejections() {
  // The order that `ASRouterTargeting.getEnvironmentSnapshot`
  // enumerates the target object matters here, but it's guaranteed to
  // be consistent by the `for ... in` ordering: see
  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/for...in#description.
  let target = {
    get foo() {
      return new Promise(resolve => resolve(1));
    },

    get bar() {
      return new Promise(resolve => {
        // Pretend that we're about to shut down.
        Services.startup.advanceShutdownPhase(
          Services.startup.SHUTDOWN_PHASE_APPSHUTDOWN
        );
        resolve(2);
      });
    },

    get baz() {
      return new Promise(resolve => resolve(3));
    },
  };

  let snapshot = await ASRouterTargeting.getEnvironmentSnapshot(target);
  // `baz` is dropped since we're shutting down by the time it's processed.
  Assert.deepEqual(snapshot, { environment: { foo: 1, bar: 2 }, version: 1 });
});
