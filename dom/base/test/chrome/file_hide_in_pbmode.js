/* global importScripts */

const isWorker = typeof DedicatedWorkerGlobalScope === "function";

function checkAll(content, inPrivateBrowsing) {
  function check(
    item,
    { valueExpected, enumerationExpected, parent = "globalThis" }
  ) {
    const exposed = valueExpected ? "is exposed" : "is not exposed";
    const pbmode = inPrivateBrowsing ? "with pbmode" : "without pbmode";
    const enumerated = enumerationExpected
      ? "is enumerated"
      : "is not enumerated";
    const worker = isWorker ? "in worker" : "in window";
    is(
      content.eval(`!!${parent}.${item}`),
      valueExpected,
      `${parent}.${item} ${exposed} ${pbmode} ${worker}`
    );
    is(
      content.eval(`"${item}" in ${parent}`),
      enumerationExpected,
      `${parent}.${item} ${enumerated} ${pbmode} ${worker}`
    );
  }

  function checkNotExposedInPBM(item, parent) {
    check(item, {
      valueExpected: !inPrivateBrowsing,
      enumerationExpected: !inPrivateBrowsing,
      parent,
    });
  }

  function checkCaches() {
    checkNotExposedInPBM("caches");
    checkNotExposedInPBM("Cache");
    checkNotExposedInPBM("CacheStorage");
  }

  function checkIDB() {
    checkNotExposedInPBM("IDBFactory");
    checkNotExposedInPBM("IDBKeyRange");
    checkNotExposedInPBM("IDBOpenDBRequest");
    checkNotExposedInPBM("IDBRequest");
    checkNotExposedInPBM("IDBVersionChangeEvent");

    // These are always accessed by jakearchibald/idb@v3 without existence checks.
    // https://github.com/jakearchibald/idb/blob/e1c7c44dbba38415745afc782b8e247da8c833f2/lib/idb.mjs#L152
    check("IDBCursor", {
      valueExpected: true,
      enumerationExpected: true,
    });
    check("IDBDatabase", {
      valueExpected: true,
      enumerationExpected: true,
    });
    check("IDBIndex", {
      valueExpected: true,
      enumerationExpected: true,
    });
    check("IDBObjectStore", {
      valueExpected: true,
      enumerationExpected: true,
    });
    check("IDBTransaction", {
      valueExpected: true,
      enumerationExpected: true,
    });

    // https://www.msn.com/feed accesses indexedDB as a global variable without existence check
    // We need to always expose the attribute itself
    check("indexedDB", {
      valueExpected: !inPrivateBrowsing,
      enumerationExpected: true,
    });
  }

  function checkSW() {
    if (isWorker) {
      // Currently not supported. Bug 1131324
      return;
    }
    checkNotExposedInPBM("serviceWorker", "navigator");
    checkNotExposedInPBM("ServiceWorker");
    checkNotExposedInPBM("ServiceWorkerContainer");
    checkNotExposedInPBM("ServiceWorkerRegistration");
    checkNotExposedInPBM("NavigationPreloadManager");
    checkNotExposedInPBM("PushManager");
    checkNotExposedInPBM("PushSubscription");
    checkNotExposedInPBM("PushSubscriptionOptions");
  }

  checkCaches();
  checkIDB();
  checkSW();
}

if (isWorker) {
  importScripts("/tests/SimpleTest/WorkerSimpleTest.js");

  globalThis.onmessage = ev => {
    const { inPrivateBrowsing } = ev.data;
    checkAll(globalThis, inPrivateBrowsing);
    postMessage({
      kind: "info",
      next: true,
      description: "Worker test finished",
    });
  };
}
