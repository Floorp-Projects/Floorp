/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Test that an error is thrown when providing invalid icon sizes
add_task(async function testInvalidIconSizes() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {},
      page_action: {},
    },

    background: function() {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        let tabId = tabs[0].id;

        let promises = [];
        for (let api of ["pageAction", "browserAction"]) {
          // helper function to run setIcon and check if it fails
          let assertSetIconThrows = function(detail, error, message) {
            detail.tabId = tabId;
            browser.test.assertThrows(
              () => browser[api].setIcon(detail),
              /an unexpected .* property/,
              "setIcon with invalid icon size"
            );
          };

          let imageData = new ImageData(1, 1);

          // test invalid icon size inputs
          for (let type of ["path", "imageData"]) {
            let img = type == "imageData" ? imageData : "test.png";

            assertSetIconThrows({ [type]: { abcdef: img } });
            assertSetIconThrows({ [type]: { "48px": img } });
            assertSetIconThrows({ [type]: { "20.5": img } });
            assertSetIconThrows({ [type]: { "5.0": img } });
            assertSetIconThrows({ [type]: { "-300": img } });
            assertSetIconThrows({ [type]: { abc: img, "5": img } });
          }

          assertSetIconThrows({
            imageData: { abcdef: imageData },
            path: { "5": "test.png" },
          });
          assertSetIconThrows({
            path: { abcdef: "test.png" },
            imageData: { "5": imageData },
          });
        }

        Promise.all(promises).then(() => {
          browser.test.notifyPass("setIcon with invalid icon size");
        });
      });
    },
  });

  await Promise.all([
    extension.startup(),
    extension.awaitFinish("setIcon with invalid icon size"),
  ]);

  await extension.unload();
});

// Test that default icon details in the manifest.json file are handled
// correctly.
add_task(async function testDefaultDetails() {
  // TODO: Test localized variants.
  let icons = [
    "foo/bar.png",
    "/foo/bar.png",
    { "19": "foo/bar.png" },
    { "38": "foo/bar.png" },
  ];

  if (window.devicePixelRatio > 1) {
    icons.push({ "19": "baz/quux.png", "38": "foo/bar.png" });
  } else {
    icons.push({ "19": "foo/bar.png", "38": "baz/quux@2x.png" });
  }

  let expectedURL = new RegExp(
    String.raw`^moz-extension://[^/]+/foo/bar\.png$`
  );

  for (let icon of icons) {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        browser_action: { default_icon: icon },
        page_action: { default_icon: icon },
      },

      background: function() {
        browser.tabs.query({ active: true, currentWindow: true }, tabs => {
          let tabId = tabs[0].id;

          browser.pageAction.show(tabId).then(() => {
            browser.test.sendMessage("ready");
          });
        });
      },

      files: {
        "foo/bar.png": imageBuffer,
        "baz/quux.png": imageBuffer,
        "baz/quux@2x.png": imageBuffer,
      },
    });

    await Promise.all([extension.startup(), extension.awaitMessage("ready")]);

    let browserActionId = makeWidgetId(extension.id) + "-browser-action";
    let pageActionId = BrowserPageActions.urlbarButtonNodeIDForActionID(
      makeWidgetId(extension.id)
    );

    await promiseAnimationFrame();

    let browserActionButton = document.getElementById(browserActionId);
    let image = getListStyleImage(browserActionButton);

    ok(
      expectedURL.test(image),
      `browser action image ${image} matches ${expectedURL}`
    );

    let pageActionImage = document.getElementById(pageActionId);
    image = getListStyleImage(pageActionImage);

    ok(
      expectedURL.test(image),
      `page action image ${image} matches ${expectedURL}`
    );

    await extension.unload();

    let node = document.getElementById(pageActionId);
    is(node, null, "pageAction image removed from document");
  }
});

// Check that attempts to load a privileged URL as an icon image fail.
add_task(async function testSecureURLsDenied() {
  // Test URLs passed to setIcon.

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {},
      page_action: {},
    },

    background: function() {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        let tabId = tabs[0].id;

        let urls = [
          "chrome://browser/content/browser.xhtml",
          "javascript:true",
        ];

        let promises = [];
        for (let url of urls) {
          for (let api of ["pageAction", "browserAction"]) {
            promises.push(
              browser.test.assertRejects(
                browser[api].setIcon({ tabId, path: url }),
                /Illegal URL/,
                `Load of '${url}' should fail.`
              )
            );
          }
        }

        Promise.all(promises).then(() => {
          browser.test.notifyPass("setIcon security tests");
        });
      });
    },
  });

  await extension.startup();

  await extension.awaitFinish("setIcon security tests");
  await extension.unload();
});

add_task(async function testSecureManifestURLsDenied() {
  // Test URLs included in the manifest.

  let urls = ["chrome://browser/content/browser.xhtml", "javascript:true"];

  let apis = ["browser_action", "page_action"];

  for (let url of urls) {
    for (let api of apis) {
      info(`TEST ${api} icon url: ${url}`);

      let matchURLForbidden = url => ({
        message: new RegExp(`match the format "strictRelativeUrl"`),
      });

      let messages = [matchURLForbidden(url)];

      let waitForConsole = new Promise(resolve => {
        // Not necessary in browser-chrome tests, but monitorConsole gripes
        // if we don't call it.
        SimpleTest.waitForExplicitFinish();

        SimpleTest.monitorConsole(resolve, messages);
      });

      let extension = ExtensionTestUtils.loadExtension({
        manifest: {
          [api]: {
            default_icon: url,
          },
        },
      });

      await Assert.rejects(
        extension.startup(),
        /startup failed/,
        "Manifest rejected"
      );

      SimpleTest.endMonitorConsole();
      await waitForConsole;
    }
  }
});
