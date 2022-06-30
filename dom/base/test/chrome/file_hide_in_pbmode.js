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
  check(content, expected, "IDBCursor");
  check(content, expected, "IDBDatabase");
  check(content, expected, "IDBFactory");
  check(content, expected, "IDBIndex");
  check(content, expected, "IDBKeyRange");
  check(content, expected, "IDBObjectStore");
  check(content, expected, "IDBOpenDBRequest");
  check(content, expected, "IDBRequest");
  check(content, expected, "IDBTransaction");
  check(content, expected, "IDBVersionChangeEvent");
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
