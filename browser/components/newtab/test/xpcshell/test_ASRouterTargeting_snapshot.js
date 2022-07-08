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
