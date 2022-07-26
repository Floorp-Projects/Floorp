/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testWindowCreate() {
  let pageExt = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: "page@mochitest" } },
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
      let window = await browser.windows.create(options);
      let win = windows.get(window.id);
      win.id = window.id;

      win.expectedTabs = Array.isArray(options.url) ? options.url.length : 1;

      return win.promise;
    }

    function createFail(options) {
      return browser.windows
        .create(options)
        .then(() => {
          browser.test.fail(`window opened with ${options.url}`);
        })
        .catch(() => {
          browser.test.succeed(`window could not open with ${options.url}`);
        });
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
        name: "Single, absolute, other extension URL",
        url: OTHER_PAGE,
        expect: [OTHER_PAGE],
      },
      {
        name: "Single protocol URL in other extension",
        url: OTHER_PROTO,
        expect: [`${OTHER_PAGE}?val=ext%2Bfoo%3Abar`],
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
    ];
    try {
      let windows = await Promise.all(
        TEST_SETS.map(t => create({ url: t.url }))
      );

      TEST_SETS.forEach((test, i) => {
        test.expect.forEach((expectUrl, x) => {
          browser.test.assertEq(
            expectUrl,
            windows[i].tabs.get(x)?.url,
            TEST_SETS[i].name
          );
        });
      });

      Promise.all(windows.map(({ id }) => browser.windows.remove(id))).then(
        () => {
          browser.test.notifyPass("window-create-url");
        }
      );

      // Expecting to fail when opening windows with multiple urls that includes
      // other extension urls.
      await Promise.all([createFail({ url: [EXTENSION_URL, OTHER_PAGE] })]);
    } catch (e) {
      browser.test.fail(`${e} :: ${e.stack}`);
      browser.test.notifyFail("window-create-url");
    }
  }

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
});
