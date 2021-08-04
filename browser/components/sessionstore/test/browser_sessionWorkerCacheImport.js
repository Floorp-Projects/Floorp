/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SessionWorkerCache } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionWorkerCache.jsm"
);

/**
 */
add_task(async function sessionWorkerCacheImport() {
  await ss.promiseAllWindowsRestored;
  await new Promise(resolve => window.requestIdleCallback(resolve));

  const baseObjs = SessionWorkerCache.getCacheObjects();
  const newObjs = [
    [10000001, "test string 1"],
    [10000002, "test string 2"],
  ];
  const expectedObjs = baseObjs.concat(newObjs);
  SessionWorkerCache.import(newObjs);
  const actualObjs = SessionWorkerCache.getCacheObjects();

  Assert.deepEqual(
    actualObjs,
    expectedObjs,
    `SessionWorkerCache contains the expected data.`
  );

  await new Promise(resolve => window.requestIdleCallback(resolve));

  const postCleanupObjs = SessionWorkerCache.getCacheObjects();

  Assert.deepEqual(
    postCleanupObjs,
    baseObjs,
    "After cleanup, SessionWorkerCache is empty."
  );
});
