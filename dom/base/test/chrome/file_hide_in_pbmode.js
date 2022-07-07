/* global importScripts */

const isWorker = typeof DedicatedWorkerGlobalScope === "function";

function check(content, expected, item) {
  const exposed = expected ? "is exposed without" : "is not exposed with";
  const worker = isWorker ? "in worker" : "in window";
  is(
    content.eval(`!!globalThis.${item}`),
    expected,
    `${item} ${exposed} pbmode ${worker}`
  );
}

function checkCaches(content, expected) {
  check(content, expected, "caches");
  check(content, expected, "Cache");
  check(content, expected, "CacheStorage");
}

function checkIDB(content, expected) {
  check(content, expected, "indexedDB");
  check(content, expected, "IDBFactory");
  check(content, expected, "IDBKeyRange");
  check(content, expected, "IDBOpenDBRequest");
  check(content, expected, "IDBRequest");
  check(content, expected, "IDBVersionChangeEvent");

  // These are always accessed by jakearchibald/idb@v3 without existence checks.
  // https://github.com/jakearchibald/idb/blob/e1c7c44dbba38415745afc782b8e247da8c833f2/lib/idb.mjs#L152
  check(content, true, "IDBCursor");
  check(content, true, "IDBDatabase");
  check(content, true, "IDBIndex");
  check(content, true, "IDBObjectStore");
  check(content, true, "IDBTransaction");
}

function checkSW(content, expected) {
  if (isWorker) {
    // Currently not supported. Bug 1131324
    return;
  }
  check(content, expected, "navigator.serviceWorker");
  check(content, expected, "ServiceWorker");
  check(content, expected, "ServiceWorkerContainer");
  check(content, expected, "ServiceWorkerRegistration");
  check(content, expected, "NavigationPreloadManager");
  check(content, expected, "PushManager");
  check(content, expected, "PushSubscription");
  check(content, expected, "PushSubscriptionOptions");
}

function checkAll(content, expected) {
  checkCaches(content, expected);
  checkIDB(content, expected);
  checkSW(content, expected);
}

if (isWorker) {
  importScripts("/tests/SimpleTest/WorkerSimpleTest.js");

  globalThis.onmessage = ev => {
    const { expected } = ev.data;
    checkAll(globalThis, expected);
    postMessage({
      kind: "info",
      next: true,
      description: "Worker test finished",
    });
  };
}
