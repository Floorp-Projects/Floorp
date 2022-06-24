"use strict";

const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });
});

function makeRunAtScript(runAt) {
  return `
    window.order ??= [];
    window.order.push("${runAt}");
    browser.test.sendMessage("injected", "order@" + window.order.join());
  `;
}

async function makeExtension(id, manifest_version, granted) {
  info(`Loading extension ` + JSON.stringify({ id, granted }));

  let manifest = {
    manifest_version,
    browser_specific_settings: { gecko: { id } },
    permissions: ["activeTab", "scripting"],
    content_scripts: [
      {
        matches: ["*://*/*"],
        js: ["static.js"],
      },
    ],
  };

  if (manifest_version === 3) {
    manifest.action = {};
  } else {
    manifest.browser_action = {};
  }

  let ext = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "temporary",

    background() {
      let expectCount = 0;

      browser.test.onMessage.addListener(async (msg, arg) => {
        if (msg === "dynamic-script") {
          await browser.scripting.registerContentScripts([arg]);
          browser.test.sendMessage("dynamic-script-done");
        } else if (msg === "injected-flush?") {
          browser.test.sendMessage("injected", "flush");
        } else if (msg === "expect-count") {
          expectCount = arg;
          browser.test.sendMessage("expect-done");
        }
      });

      let action = browser.action || browser.browserAction;

      action.onClicked.addListener(tab => {
        browser.scripting.executeScript({
          target: { tabId: tab.id },
          func: expectCount => {
            let retryCount = 0;

            function tryScriptCount() {
              let id = browser.runtime.id.split("@")[0];
              let count = document.body.dataset[id] | 0;
              if (count < expectCount && retryCount < 100) {
                // This needs to run after all scripts, to confirm the correct
                // number of scripts was injected. The two paths are inherently
                // independant, and since there's a variable number of content
                // scripts, there's no easy/better way to do it than a delay
                // and retry for up to 100 frames.

                // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
                setTimeout(tryScriptCount, 30);
                retryCount++;
                return;
              }
              browser.test.sendMessage("scriptCount", count);
            }

            tryScriptCount();
          },
          args: [expectCount],
        });
      });
    },

    files: {
      "static.js"() {
        // Need to use DOM attributes (or the dataset), because two different
        // content script sandboxes (from top frame and the same-origin iframe)
        // get different wrappers, they don't see each other's top expandos.

        // Need to avoid using the @ character (from the extension id)
        // because it's not allowed as part of the DOM attribute name.

        let id = browser.runtime.id.split("@")[0];
        top.document.body.dataset[id] = (top.document.body.dataset[id] | 0) + 1;

        browser.test.log(
          `Static content script from ${id} running on ${location.href}.`
        );

        browser.test.sendMessage("injected", "static@" + location.host);
      },
      "dynamic.js"() {
        let id = browser.runtime.id.split("@")[0];
        top.document.body.dataset[id] = (top.document.body.dataset[id] | 0) + 1;

        browser.test.log(
          `Dynamic content script from ${id} running on ${location.href}.`
        );

        let frame = window === top ? "top" : "frame";
        browser.test.sendMessage(
          "injected",
          `dynamic-${frame}@${location.host}`
        );
      },
      "document_start.js": makeRunAtScript("document_start"),
      "document_end.js": makeRunAtScript("document_end"),
      "document_idle.js": makeRunAtScript("document_idle"),
    },
  });

  if (granted) {
    info("Granting initial permissions.");
    await ExtensionPermissions.add(id, { permissions: [], origins: granted });
  }

  await ext.startup();
  return ext;
}

async function testActiveScript(extension, expectCount, expectHosts) {
  info(`Testing ${extension.id} on ${gBrowser.currentURI.spec}.`);

  extension.sendMessage("expect-count", expectCount);
  await extension.awaitMessage("expect-done");

  await clickBrowserAction(extension);

  let received = [];
  for (let host of expectHosts) {
    info(`Waiting for a script to run in a ${host} frame.`);
    received.push(await extension.awaitMessage("injected"));
  }

  extension.sendMessage("injected-flush?");
  info("Waiting for the flush message between test runs.");
  let flush = await extension.awaitMessage("injected");
  is(flush, "flush", "Messages properly flushed.");

  is(received.sort().join(), expectHosts.join(), "All messages received.");

  info(`Awaiting the counter from the activeTab content script.`);
  let scriptCount = await extension.awaitMessage("scriptCount");
  is(scriptCount | 0, expectCount, "Expected number of scripts running");
}

add_task(async function test_action_activeScript() {
  // Static MV2 extension content scripts are not affected.
  let ext0 = await makeExtension("ext0@test", 2, ["*://example.com/*"]);

  let ext1 = await makeExtension("ext1@test", 3);
  let ext2 = await makeExtension("ext2@test", 3, ["*://example.com/*"]);
  let ext3 = await makeExtension("ext3@test", 3, ["*://mochi.test/*"]);

  // Test run_at script ordering.
  let ext4 = await makeExtension("ext4@test", 3);

  await BrowserTestUtils.withNewTab("about:blank", async () => {
    info("No content scripts run on top level about:blank.");
    await testActiveScript(ext0, 0, []);
    await testActiveScript(ext1, 0, []);
    await testActiveScript(ext2, 0, []);
    await testActiveScript(ext3, 0, []);
    await testActiveScript(ext4, 0, []);
  });

  let dynamicScript = {
    id: "script",
    js: ["dynamic.js"],
    matches: ["<all_urls>"],
    allFrames: true,
    persistAcrossSessions: false,
  };

  // MV2 extensions don't support activeScript. This dynamic script won't run
  // when action button is clicked, but will run on example.com automatically.
  ext0.sendMessage("dynamic-script", dynamicScript);
  await ext0.awaitMessage("dynamic-script-done");

  // Only ext3 will have a dynamic script, matching <all_urls> with allFrames.
  ext3.sendMessage("dynamic-script", dynamicScript);
  await ext3.awaitMessage("dynamic-script-done");

  let url =
    "https://example.com/browser/browser/components/extensions/test/browser/file_with_xorigin_frame.html";

  await BrowserTestUtils.withNewTab(url, async browser => {
    info("ext0 is MV2, static content script should run automatically.");
    info("ext0 has example.com permission, dynamic scripts should also run.");
    let received = [
      await ext0.awaitMessage("injected"),
      await ext0.awaitMessage("injected"),
      await ext0.awaitMessage("injected"),
    ];
    is(
      received.sort().join(),
      "dynamic-frame@example.com,dynamic-top@example.com,static@example.com",
      "All messages received"
    );

    info("Clicking ext0 button should not run content script again.");
    await testActiveScript(ext0, 3, []);

    info("ext2 has host permission, content script should run automatically.");
    let static2 = await ext2.awaitMessage("injected");
    is(static2, "static@example.com", "Script ran automatically");

    info("Clicking ext2 button should not run content script again.");
    await testActiveScript(ext2, 1, []);

    await testActiveScript(ext1, 1, ["static@example.com"]);

    await testActiveScript(ext3, 3, [
      "dynamic-frame@example.com",
      "dynamic-top@example.com",
      "static@example.com",
    ]);

    await testActiveScript(ext4, 1, ["static@example.com"]);

    // Navigate same-origin iframe to another page, activeScripts shouldn't run.
    let bc = browser.browsingContext.children[0].children[0];
    SpecialPowers.spawn(bc, [], () => {
      content.location.href = "file_dummy.html";
    });
    // But dynamic script from ext0 should run automatically again.
    let dynamic0 = await ext0.awaitMessage("injected");
    is(dynamic0, "dynamic-frame@example.com", "Script ran automatically");

    info("Clicking all buttons again should not activeScripts.");
    await testActiveScript(ext0, 4, []);
    await testActiveScript(ext1, 1, []);
    await testActiveScript(ext2, 1, []);
    // Except ext3 dynamic allFrames script runs in the new navigated page.
    await testActiveScript(ext3, 4, ["dynamic-frame@example.com"]);
    await testActiveScript(ext4, 1, []);
  });

  // Register run_at content scripts in reverse order.
  for (let runAt of ["document_idle", "document_end", "document_start"]) {
    ext4.sendMessage("dynamic-script", {
      id: runAt,
      runAt: runAt,
      js: [`${runAt}.js`],
      matches: ["http://mochi.test/*"],
      persistAcrossSessions: false,
    });
    await ext4.awaitMessage("dynamic-script-done");
  }

  await BrowserTestUtils.withNewTab("http://mochi.test:8888/", async () => {
    info("ext0 is MV2, static content script should run automatically.");
    let static0 = await ext0.awaitMessage("injected");
    is(static0, "static@mochi.test:8888", "Script ran automatically.");

    info("Clicking ext0 button should not run content script again.");
    await testActiveScript(ext0, 1, []);

    info("ext3 has host permission, content script should run automatically.");
    let received3 = [
      await ext3.awaitMessage("injected"),
      await ext3.awaitMessage("injected"),
    ];
    is(
      received3.sort().join(),
      "dynamic-top@mochi.test:8888,static@mochi.test:8888",
      "All messages received."
    );

    info("Clicking ext3 button should not run content script again.");
    await testActiveScript(ext3, 2, []);

    await testActiveScript(ext1, 1, ["static@mochi.test:8888"]);
    await testActiveScript(ext2, 1, ["static@mochi.test:8888"]);

    // Expect run_at content scripts to run in the correct order.
    await testActiveScript(ext4, 1, [
      "order@document_start",
      "order@document_start,document_end",
      "order@document_start,document_end,document_idle",
      "static@mochi.test:8888",
    ]);

    info("Clicking all buttons again should not run content scripts.");
    await testActiveScript(ext0, 1, []);
    await testActiveScript(ext1, 1, []);
    await testActiveScript(ext2, 1, []);
    await testActiveScript(ext3, 2, []);
    await testActiveScript(ext4, 1, []);
  });

  await ext0.unload();
  await ext1.unload();
  await ext2.unload();
  await ext3.unload();
  await ext4.unload();
});
