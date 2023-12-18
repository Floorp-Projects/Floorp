/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testWindowCreate() {
  let pageExt = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: { gecko: { id: "page@mochitest" } },
      protocol_handlers: [
        {
          protocol: "ext+foo",
          name: "a foo protocol handler",
          uriTemplate: "page.html?val=%s",
        },
      ],
    },
    files: {
      "page.html": `<html><head>
          <meta charset="utf-8">
        </head></html>`,
    },
  });
  await pageExt.startup();

  async function background(OTHER_PAGE) {
    browser.test.log(`== using ${OTHER_PAGE}`);
    const EXTENSION_URL = browser.runtime.getURL("test.html");
    const EXT_PROTO = "ext+bar:foo";
    const OTHER_PROTO = "ext+foo:bar";

    let windows = new (class extends Map {
      // eslint-disable-line new-parens
      get(id) {
        if (!this.has(id)) {
          let window = {
            tabs: new Map(),
          };
          window.promise = new Promise(resolve => {
            window.resolvePromise = resolve;
          });

          this.set(id, window);
        }

        return super.get(id);
      }
    })();

    browser.tabs.onUpdated.addListener((tabId, changed, tab) => {
      if (changed.status == "complete" && tab.url !== "about:blank") {
        let window = windows.get(tab.windowId);
        window.tabs.set(tab.index, tab);

        if (window.tabs.size === window.expectedTabs) {
          browser.test.log("resolving a window load");
          window.resolvePromise(window);
        }
      }
    });

    async function create(options) {
      browser.test.log(`creating window for ${options.url}`);
      // Note: may reject
      let window = await browser.windows.create(options);
      let win = windows.get(window.id);
      win.id = window.id;

      win.expectedTabs = Array.isArray(options.url) ? options.url.length : 1;

      return win.promise;
    }

    let TEST_SETS = [
      {
        name: "Single protocol URL in this extension",
        url: EXT_PROTO,
        expect: [`${EXTENSION_URL}?val=ext%2Bbar%3Afoo`],
      },
      {
        name: "Single, relative URL",
        url: "test.html",
        expect: [EXTENSION_URL],
      },
      {
        name: "Single, absolute, extension URL",
        url: EXTENSION_URL,
        expect: [EXTENSION_URL],
      },
      {
        // This is primarily for backwards-compatibility, to allow extensions
        // to open other home pages. This test case opens the home page
        // explicitly; the implicit case (windows.create({}) without URL) is at
        // browser_ext_chrome_settings_overrides_home.js.
        name: "Single, absolute, other extension URL",
        url: OTHER_PAGE,
        expect: [OTHER_PAGE],
      },
      {
        // This is oddly inconsistent with the non-array case, but here we are
        // intentionally stricter because of lesser backwards-compatibility
        // concerns.
        name: "Array, absolute, other extension URL",
        url: [OTHER_PAGE],
        expectError: `Illegal URL: ${OTHER_PAGE}`,
      },
      {
        name: "Single protocol URL in other extension",
        url: OTHER_PROTO,
        expect: [`${OTHER_PAGE}?val=ext%2Bfoo%3Abar`],
      },
      {
        name: "Single, about:blank",
        // Added "?" after "about:blank" because the test's tab load detection
        // ignores about:blank.
        url: "about:blank?",
        expect: ["about:blank?"],
      },
      {
        name: "multiple urls",
        url: [EXT_PROTO, "test.html", EXTENSION_URL, OTHER_PROTO],
        expect: [
          `${EXTENSION_URL}?val=ext%2Bbar%3Afoo`,
          EXTENSION_URL,
          EXTENSION_URL,
          `${OTHER_PAGE}?val=ext%2Bfoo%3Abar`,
        ],
      },
      {
        name: "Reject array of own allowed URLs and other moz-extension:-URL",
        url: [EXTENSION_URL, EXT_PROTO, "about:blank?#", OTHER_PAGE],
        expectError: `Illegal URL: ${OTHER_PAGE}`,
      },
      {
        name: "Single, about:robots",
        url: "about:robots",
        expectError: "Illegal URL: about:robots",
      },
      {
        name: "Array containing about:robots",
        url: ["about:robots"],
        expectError: "Illegal URL: about:robots",
      },
    ];
    async function checkCreateResult({ status, value, reason }, testCase) {
      const window = status === "fulfilled" ? value : null;
      try {
        if (testCase.expectError) {
          let error = reason?.message;
          browser.test.assertEq(testCase.expectError, error, testCase.name);
        } else {
          let tabUrls = [];
          for (let [tabIndex, tab] of window.tabs) {
            tabUrls[tabIndex] = tab.url;
          }
          browser.test.assertDeepEq(testCase.expect, tabUrls, testCase.name);
        }
      } catch (e) {
        browser.test.fail(`Unexpected failure in ${testCase.name} :: ${e}`);
      } finally {
        // Close opened windows, whether they were expected or not.
        if (window) {
          await browser.windows.remove(window.id);
        }
      }
    }
    try {
      let promises = [];
      for (let testSet of TEST_SETS) {
        try {
          let testPromise = create({ url: testSet.url });
          promises.push(testPromise);
          // Bug 1869385 - we need to await each window opening sequentially.
          // The events we check for depend on paint finishing,
          // which won't happen if the window is occluded before it finishes
          // loading.
          await testPromise;
        } catch (e) {
          // Some of these calls are expected to fail,
          // which we verify when calling checkCreateResult below.
        }
      }
      const results = await Promise.allSettled(promises);
      await Promise.all(
        TEST_SETS.map((t, i) => checkCreateResult(results[i], t))
      );
      browser.test.notifyPass("window-create-url");
    } catch (e) {
      browser.test.fail(`${e} :: ${e.stack}`);
      browser.test.notifyFail("window-create-url");
    }
  }

  // Watch for any permission prompts to show up and accept them.
  let dialogCount = 0;
  let windowObserver = window => {
    // This listener will go away when the window is closed so there is no need
    // to explicitely remove it.
    // eslint-disable-next-line mozilla/balanced-listeners
    window.addEventListener("dialogopen", event => {
      dialogCount++;
      let { dialog } = event.detail;
      Assert.equal(
        dialog?._openedURL,
        "chrome://mozapps/content/handling/permissionDialog.xhtml",
        "Should only ever see the permission dialog"
      );
      let dialogEl = dialog._frame.contentDocument.querySelector("dialog");
      Assert.ok(dialogEl, "Dialog element should exist");
      dialogEl.setAttribute("buttondisabledaccept", false);
      dialogEl.acceptDialog();
    });
  };
  Services.obs.addObserver(windowObserver, "browser-delayed-startup-finished");
  registerCleanupFunction(() => {
    Services.obs.removeObserver(
      windowObserver,
      "browser-delayed-startup-finished"
    );
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
      protocol_handlers: [
        {
          protocol: "ext+bar",
          name: "a bar protocol handler",
          uriTemplate: "test.html?val=%s",
        },
      ],
    },

    background: `(${background})("moz-extension://${pageExt.uuid}/page.html")`,

    files: {
      "test.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head><body></body></html>`,
    },
  });

  await extension.startup();
  await extension.awaitFinish("window-create-url");
  await extension.unload();
  await pageExt.unload();

  Assert.equal(
    dialogCount,
    2,
    "Expected to see the right number of permission prompts."
  );

  // Make sure windows have been released before finishing.
  Cu.forceGC();
});
