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
  deepEqual(snapshot, { environment: { foo: 1 }, version: 1 });
});

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
  deepEqual(snapshot, { environment: { foo: 1, bar: 2 }, version: 1 });
});
