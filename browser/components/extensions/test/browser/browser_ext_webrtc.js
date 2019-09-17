"use strict";

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.navigator.permission.fake", true],
    ],
  });
});

add_task(async function test_background_request() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {},
    async background() {
      browser.test.onMessage.addListener(async msg => {
        if (msg.type != "testGUM") {
          browser.test.fail("unknown message");
        }

        await browser.test.assertRejects(
          navigator.mediaDevices.getUserMedia({ audio: true }),
          /The request is not allowed/,
          "Calling gUM in background pages throws an error"
        );
        browser.test.notifyPass("done");
      });
    },
  });

  await extension.startup();

  let policy = WebExtensionPolicy.getByID(extension.id);
  let principal = policy.extension.principal;
  // Add a permission for the extension to make sure that we throw even
  // if permission was given.
  PermissionTestUtils.add(principal, "microphone", Services.perms.ALLOW_ACTION);

  let finished = extension.awaitFinish("done");
  extension.sendMessage({ type: "testGUM" });
  await finished;

  PermissionTestUtils.remove(principal, "microphone");
  await extension.unload();
});

let scriptPage = url =>
  `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

add_task(async function test_popup_request() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        browser_style: true,
      },
    },

    files: {
      "popup.html": scriptPage("popup.js"),
      "popup.js": function() {
        browser.test
          .assertRejects(
            navigator.mediaDevices.getUserMedia({ audio: true }),
            /The request is not allowed/,
            "Calling gUM in popup pages without permission throws an error"
          )
          .then(function() {
            browser.test.notifyPass("done");
          });
      },
    },
  });

  await extension.startup();
  clickBrowserAction(extension);
  await extension.awaitFinish("done");
  await extension.unload();

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      // Use the same url for background page and browserAction popup,
      // to double-check that the page url is not being used to decide
      // if webRTC requests should be allowed or not.
      background: { page: "page.html" },
      browser_action: {
        default_popup: "page.html",
        browser_style: true,
      },
    },

    files: {
      "page.html": scriptPage("page.js"),
      "page.js": async function() {
        const isBackgroundPage =
          window == (await browser.runtime.getBackgroundPage());

        if (isBackgroundPage) {
          await browser.test.assertRejects(
            navigator.mediaDevices.getUserMedia({ audio: true }),
            /The request is not allowed/,
            "Calling gUM in background pages throws an error"
          );
        } else {
          try {
            await navigator.mediaDevices.getUserMedia({ audio: true });
            browser.test.notifyPass("done");
          } catch (err) {
            browser.test.fail(`Failed with error ${err.message}`);
            browser.test.notifyFail("done");
          }
        }
      },
    },
  });

  // Add a permission for the extension to make sure that we throw even
  // if permission was given.
  await extension.startup();

  let policy = WebExtensionPolicy.getByID(extension.id);
  let principal = policy.extension.principal;

  PermissionTestUtils.add(principal, "microphone", Services.perms.ALLOW_ACTION);
  clickBrowserAction(extension);

  await extension.awaitFinish("done");
  PermissionTestUtils.remove(principal, "microphone");
  await extension.unload();
});
