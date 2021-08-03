/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SessionWorkerCache } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionWorkerCache.jsm"
);

/**
 */
add_task(async function sessionWorkerCacheImport() {
  const expectedObjs = [
    [1, "test string 1"],
    [2, "test string 2"],
  ];
  SessionWorkerCache.import(expectedObjs);
  const actualObjs = SessionWorkerCache.getCacheObjects();

  Assert.deepEqual(
    actualObjs,
    expectedObjs,
    `SessionWorkerCache contains the expected data.`
  );

  await new Promise(resolve => window.requestIdleCallback(resolve));
  await ss.promiseAllWindowsRestored;

  const postCleanupObjs = SessionWorkerCache.getCacheObjects();

  Assert.deepEqual(
    postCleanupObjs,
    [],
    "After cleanup, SessionWorkerCache is empty."
  );
});
