/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testInNonShared() {
  const ns1 = ChromeUtils.importESModule("resource://test/esmified-1.sys.mjs");

  const ns2 = ChromeUtils.importESModule("resource://test/esmified-1.sys.mjs", {
    global: "contextual",
  });

  Assert.equal(ns1, ns2);
  Assert.equal(ns1.obj, ns2.obj);
});

add_task(async function testInShared() {
  const { ns: ns1 } = ChromeUtils.importESModule("resource://test/contextual.sys.mjs");

  const ns2 = ChromeUtils.importESModule("resource://test/esmified-1.sys.mjs", {
    global: "shared",
  });

  Assert.equal(ns1, ns2);
  Assert.equal(ns1.obj, ns2.obj);
});

add_task(async function testInShared() {
  const { ns: ns1 } = ChromeUtils.importESModule("resource://test/contextual.sys.mjs", {
    global: "devtools",
  });

  const ns2 = ChromeUtils.importESModule("resource://test/esmified-1.sys.mjs", {
    global: "devtools",
  });

  Assert.equal(ns1, ns2);
  Assert.equal(ns1.obj, ns2.obj);
});

add_task(async function testInWorker() {
  const worker = new ChromeWorker("resource://test/contextual_worker.js");
  const { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage("");

  const result = await promise;

  Assert.ok(result.equal1);
  Assert.ok(result.equal2);
});
