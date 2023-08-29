/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);

const { createHttpServer } = AddonTestUtils;

// Force ServiceWorkerRegistrar to init by calling do_get_profile.
// (This has to be called before AddonTestUtils.init, because it does
// also call do_get_profile internally but it doesn't notify
// profile-after-change).
do_get_profile(true);

AddonTestUtils.init(this);
ExtensionTestUtils.init(this);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("dummy page");
});

add_setup(() => {
  info("Making sure Services.io.offline is true");
  // Explicitly setting Services.io.offline to true makes this test able
  // to hit on Desktop builds the same issue that test_ext_cache_api.js
  // was hitting on Android builds (Bug 1844825).
  Services.io.offline = true;
});

// Regression test derived from Bug 1844825.
add_task(async function test_fetch_request_from_ext_shared_worker() {
  if (!WebExtensionPolicy.useRemoteWebExtensions) {
    // Ensure RemoteWorkerService has been initialized in the main
    // process.
    Services.obs.notifyObservers(null, "profile-after-change");
  }

  const background = async function () {
    const testUrl = `http://example.com/dummy`;
    const worker = new SharedWorker("worker.js");
    const { data: result } = await new Promise(resolve => {
      worker.port.onmessage = resolve;
      worker.port.postMessage(["worker-fetch-test", testUrl]);
    });

    browser.test.sendMessage("test-sharedworker-fetch:done", result);
  };

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: { permissions: ["http://example.com/*"] },
    files: {
      "worker.js": function () {
        self.onconnect = evt => {
          const port = evt.ports[0];
          port.onmessage = async evt => {
            let result = {};
            let message;
            try {
              const [msg, url] = evt.data;
              message = msg;
              const response = await fetch(url);
              dump(`fetch call resolved: ${response}\n`);
              result.fetchResolvesTo = `${response}`;
            } catch (err) {
              dump(`fetch call rejected: ${err}\n`);
              result.error = err.name;
              throw err;
            } finally {
              port.postMessage([`${message}:result`, result]);
            }
          };
        };
      },
    },
  });

  await extension.startup();
  const result = await extension.awaitMessage("test-sharedworker-fetch:done");
  if (Services.io.offline && WebExtensionPolicy.useRemoteWebExtensions) {
    // If the network is offline and the extensions are running in the
    // child extension process, expect the fetch call to be rejected
    // with an TypeError.
    Assert.deepEqual(
      ["worker-fetch-test:result", { error: "TypeError" }],
      result,
      "fetch should have been rejected with an TypeError"
    );
  } else {
    // If the network is not offline or the extension are running in the
    // parent process, we expect the fetch call to resolve to a Response.
    Assert.deepEqual(
      ["worker-fetch-test:result", { fetchResolvesTo: "[object Response]" }],
      result,
      "fetch should have been resolved to a Response instance"
    );
  }
  await extension.unload();
});
