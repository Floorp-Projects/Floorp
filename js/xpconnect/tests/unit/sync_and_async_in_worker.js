onmessage = async event => {
  if (event.data.order === "test") {
    globalThis["loaded"] = [];
    const ns = await import("resource://test/non_shared_1.mjs");
    postMessage({});
    return;
  }

  if (event.data.order === "sync-before-async") {
    globalThis["loaded"] = [];
    const ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
      global: "current",
    });

    const sync_beforeInc = ns.getCounter();
    ns.incCounter();
    const sync_afterInc = ns.getCounter();

    const loaded1 = globalThis["loaded"].join(",");

    let nsPromise;
    if (event.data.target === "top-level") {
      nsPromise = import("resource://test/non_shared_1.mjs");
    } else {
      nsPromise = import("resource://test/import_non_shared_1.mjs");
    }

    const ns2 = await nsPromise;

    const async_beforeInc = ns2.getCounter();
    ns2.incCounter();
    const async_afterInc = ns2.getCounter();
    const sync_afterIncInc = ns.getCounter();

    const loaded2 = globalThis["loaded"].join(",");

    postMessage({
      sync_beforeInc,
      sync_afterInc,
      sync_afterIncInc,
      async_beforeInc,
      async_afterInc,
      loaded1,
      loaded2,
    });
    return;
  }

  if (event.data.order === "sync-after-async") {
    globalThis["loaded"] = [];
    const ns = await import("resource://test/non_shared_1.mjs");

    const async_beforeInc = ns.getCounter();
    ns.incCounter();
    const async_afterInc = ns.getCounter();

    const loaded1 = globalThis["loaded"].join(",");

    let ns2;
    if (event.data.target === "top-level") {
      ns2 = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
        global: "current",
      });
    } else {
      ns2 = ChromeUtils.importESModule("resource://test/import_non_shared_1.mjs", {
        global: "current",
      });
    }

    const sync_beforeInc = ns2.getCounter();
    ns2.incCounter();
    const sync_afterInc = ns2.getCounter();
    const async_afterIncInc = ns.getCounter();

    const loaded2 = globalThis["loaded"].join(",");

    postMessage({
      sync_beforeInc,
      sync_afterInc,
      async_beforeInc,
      async_afterInc,
      async_afterIncInc,
      loaded1,
      loaded2,
    });
    return;
  }

  if (event.data.order === "sync-while-async") {
    globalThis["loaded"] = [];
    const nsPromise = import("resource://test/non_shared_1.mjs");

    let errorMessage = "";
    try {
      if (event.data.target === "top-level") {
        ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
          global: "current",
        });
      } else {
        ChromeUtils.importESModule("resource://test/import_non_shared_1.mjs", {
          global: "current",
        });
      }
    } catch (e) {
      errorMessage = e.message;
    }

    const ns = await nsPromise;

    const async_beforeInc = ns.getCounter();
    ns.incCounter();
    const async_afterInc = ns.getCounter();

    const loaded = globalThis["loaded"].join(",");

    postMessage({
      sync_error: errorMessage,
      async_beforeInc,
      async_afterInc,
      loaded,
    });
    return;
  }
};
