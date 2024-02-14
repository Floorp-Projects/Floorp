/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testWorker() {
  const win1 = createChromeWindow();

  const worker = new ChromeWorker("resource://test/non_shared_worker_1.js");
  const { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage("");

  const result = await promise;

  Assert.equal(result.c1, 0);
  Assert.equal(result.c2, 1);
  Assert.equal(result.loaded, "2,1");
});

add_task(async function testSyncImportBeforeAsyncImportTopLevelInWorker() {
  const window = createChromeWindow();

  let worker = new ChromeWorker("resource://test/sync_and_async_in_worker.js");
  let { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage({ order: "sync-before-async", target: "top-level" });

  const {
    sync_beforeInc,
    sync_afterInc,
    sync_afterIncInc,
    async_beforeInc,
    async_afterInc,
    loaded1,
    loaded2,
  } = await promise;

  Assert.equal(sync_beforeInc, 0);
  Assert.equal(sync_afterInc, 1);

  Assert.equal(loaded1, "2,1");

  Assert.equal(async_beforeInc, 1);
  Assert.equal(async_afterInc, 2);
  Assert.equal(sync_afterIncInc, 2);

  Assert.equal(loaded2, "2,1");
});

add_task(async function testSyncImportBeforeAsyncImportDependencyInWorker() {
  let worker = new ChromeWorker("resource://test/sync_and_async_in_worker.js");
  let { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage({ order: "sync-before-async", target: "dependency" });

  const {
    sync_beforeInc,
    sync_afterInc,
    sync_afterIncInc,
    async_beforeInc,
    async_afterInc,
    loaded1,
    loaded2,
  } = await promise;

  Assert.equal(sync_beforeInc, 0);
  Assert.equal(sync_afterInc, 1);

  Assert.equal(loaded1, "2,1");

  Assert.equal(async_beforeInc, 1);
  Assert.equal(async_afterInc, 2);
  Assert.equal(sync_afterIncInc, 2);

  Assert.equal(loaded2, "2,1");
});

add_task(async function testSyncImportAfterAsyncImportTopLevelInWorker() {
  const window = createChromeWindow();

  let worker = new ChromeWorker("resource://test/sync_and_async_in_worker.js");
  let { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage({ order: "sync-after-async", target: "top-level" });

  const {
    sync_beforeInc,
    sync_afterInc,
    async_beforeInc,
    async_afterInc,
    async_afterIncInc,
    loaded1,
    loaded2,
  } = await promise;

  Assert.equal(async_beforeInc, 0);
  Assert.equal(async_afterInc, 1);

  Assert.equal(loaded1, "2,1");

  Assert.equal(sync_beforeInc, 1);
  Assert.equal(sync_afterInc, 2);
  Assert.equal(async_afterIncInc, 2);

  Assert.equal(loaded2, "2,1");
});

add_task(async function testSyncImportAfterAsyncImportDependencyInWorker() {
  const window = createChromeWindow();

  let worker = new ChromeWorker("resource://test/sync_and_async_in_worker.js");
  let { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage({ order: "sync-after-async", target: "dependency" });

  const {
    sync_beforeInc,
    sync_afterInc,
    async_beforeInc,
    async_afterInc,
    async_afterIncInc,
    loaded1,
    loaded2,
  } = await promise;

  Assert.equal(async_beforeInc, 0);
  Assert.equal(async_afterInc, 1);

  Assert.equal(loaded1, "2,1");

  Assert.equal(sync_beforeInc, 1);
  Assert.equal(sync_afterInc, 2);
  Assert.equal(async_afterIncInc, 2);

  Assert.equal(loaded2, "2,1");
});

add_task(async function testSyncImportWhileAsyncImportTopLevelInWorker() {
  const window = createChromeWindow();

  let worker = new ChromeWorker("resource://test/sync_and_async_in_worker.js");
  let { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage({ order: "sync-while-async", target: "top-level" });

  const {
    sync_error,
    async_beforeInc,
    async_afterInc,
    loaded,
  } = await promise;

  Assert.stringMatches(sync_error, /ChromeUtils.importESModule cannot be used/);

  Assert.equal(async_beforeInc, 0);
  Assert.equal(async_afterInc, 1);

  Assert.equal(loaded, "2,1");
});

add_task(async function testSyncImportWhileAsyncImportDependencyInWorker() {
  const window = createChromeWindow();

  let worker = new ChromeWorker("resource://test/sync_and_async_in_worker.js");
  let { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage({ order: "sync-while-async", target: "dependency" });

  const {
    sync_error,
    async_beforeInc,
    async_afterInc,
    loaded,
  } = await promise;

  Assert.stringMatches(sync_error, /ChromeUtils.importESModule cannot be used/);

  Assert.equal(async_beforeInc, 0);
  Assert.equal(async_afterInc, 1);

  Assert.equal(loaded, "2,1");
});
