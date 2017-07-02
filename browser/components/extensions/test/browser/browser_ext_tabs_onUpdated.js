/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

requestLongerTimeout(2);

add_task(async function() {
  let win1 = await BrowserTestUtils.openNewBrowserWindow();

  await focusWindow(win1);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
      "content_scripts": [{
        "matches": ["http://mochi.test/*/context_tabs_onUpdated_page.html"],
        "js": ["content-script.js"],
        "run_at": "document_start",
      }],
    },

    background: function() {
      let pageURL = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context_tabs_onUpdated_page.html";

      let expectedSequence = [
        {status: "loading"},
        {status: "loading", url: pageURL},
        {status: "complete"},
      ];
      let collectedSequence = [];

      browser.tabs.onUpdated.addListener(function(tabId, updatedInfo) {
        // onUpdated also fires with updatedInfo.faviconUrl, so explicitly
        // check for updatedInfo.status before recording the event.
        if ("status" in updatedInfo) {
          collectedSequence.push(updatedInfo);
        }
      });

      browser.runtime.onMessage.addListener(function() {
        if (collectedSequence.length !== expectedSequence.length) {
          browser.test.assertEq(
            JSON.stringify(expectedSequence),
            JSON.stringify(collectedSequence),
            "got unexpected number of updateInfo data"
          );
        } else {
          for (let i = 0; i < expectedSequence.length; i++) {
            browser.test.assertEq(
              expectedSequence[i].status,
              collectedSequence[i].status,
              "check updatedInfo status"
            );
            if (expectedSequence[i].url || collectedSequence[i].url) {
              browser.test.assertEq(
                expectedSequence[i].url,
                collectedSequence[i].url,
                "check updatedInfo url"
              );
            }
          }
        }

        browser.test.notifyPass("tabs.onUpdated");
      });

      browser.tabs.create({url: pageURL});
    },
    files: {
      "content-script.js": `
        window.addEventListener("message", function(evt) {
          if (evt.data == "frame-updated") {
            browser.runtime.sendMessage("load-completed");
          }
        }, true);
      `,
    },
  });

  await Promise.all([
    extension.startup(),
    extension.awaitFinish("tabs.onUpdated"),
  ]);

  await extension.unload();

  await BrowserTestUtils.closeWindow(win1);
});

async function do_test_update(background, withPermissions = true) {
  let win1 = await BrowserTestUtils.openNewBrowserWindow();

  await focusWindow(win1);

  let manifest = {};
  if (withPermissions) {
    manifest.permissions = ["tabs", "http://mochi.test/"];
  }
  let extension = ExtensionTestUtils.loadExtension({manifest, background});

  await Promise.all([
    await extension.startup(),
    await extension.awaitFinish("finish"),
  ]);

  await extension.unload();

  await BrowserTestUtils.closeWindow(win1);
}

add_task(async function test_pinned() {
  await do_test_update(function background() {
    // Create a new tab for testing update.
    browser.tabs.create({}, function(tab) {
      browser.tabs.onUpdated.addListener(function onUpdated(tabId, changeInfo) {
        // Check callback
        browser.test.assertEq(tabId, tab.id, "Check tab id");
        browser.test.log("onUpdate: " + JSON.stringify(changeInfo));
        if ("pinned" in changeInfo) {
          browser.test.assertTrue(changeInfo.pinned, "Check changeInfo.pinned");
          browser.tabs.onUpdated.removeListener(onUpdated);
          // Remove created tab.
          browser.tabs.remove(tabId);
          browser.test.notifyPass("finish");
        }
      });
      browser.tabs.update(tab.id, {pinned: true});
    });
  });
});

add_task(async function test_unpinned() {
  await do_test_update(function background() {
    // Create a new tab for testing update.
    browser.tabs.create({pinned: true}, function(tab) {
      browser.tabs.onUpdated.addListener(function onUpdated(tabId, changeInfo) {
        // Check callback
        browser.test.assertEq(tabId, tab.id, "Check tab id");
        browser.test.log("onUpdate: " + JSON.stringify(changeInfo));
        if ("pinned" in changeInfo) {
          browser.test.assertFalse(changeInfo.pinned, "Check changeInfo.pinned");
          browser.tabs.onUpdated.removeListener(onUpdated);
          // Remove created tab.
          browser.tabs.remove(tabId);
          browser.test.notifyPass("finish");
        }
      });
      browser.tabs.update(tab.id, {pinned: false});
    });
  });
});

add_task(async function test_url() {
  await do_test_update(function background() {
    // Create a new tab for testing update.
    browser.tabs.create({}, function(tab) {
      browser.tabs.onUpdated.addListener(function onUpdated(tabId, changeInfo) {
        if ("url" in changeInfo) {
          // When activity stream is enabled, about:newtab runs in the content process
          // which causes some timing issues for onUpdated. So if we encounter
          // about:newtab, return early and continue waiting for about:blank.
          if (changeInfo.url === "about:newtab") {
            return;
          }
          browser.test.assertEq("about:blank", changeInfo.url,
                                "Check changeInfo.url");
          browser.tabs.onUpdated.removeListener(onUpdated);
          // Remove created tab.
          browser.tabs.remove(tabId);
          browser.test.notifyPass("finish");
        }
        // Check callback
        browser.test.assertEq(tabId, tab.id, "Check tab id");
        browser.test.log("onUpdate: " + JSON.stringify(changeInfo));
      });
      browser.tabs.update(tab.id, {url: "about:blank"});
    });
  });
});

add_task(async function test_title() {
  await do_test_update(async function background() {
    const url = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context_tabs_onUpdated_page.html";
    const tab = await browser.tabs.create({url});

    browser.tabs.onUpdated.addListener(function onUpdated(tabId, changeInfo) {
      browser.test.assertEq(tabId, tab.id, "Check tab id");
      browser.test.log(`onUpdated: ${JSON.stringify(changeInfo)}`);
      if ("title" in changeInfo && changeInfo.title === "New Message (1)") {
        browser.test.log("changeInfo.title is correct");
        browser.tabs.onUpdated.removeListener(onUpdated);
        browser.tabs.remove(tabId);
        browser.test.notifyPass("finish");
      }
    });

    browser.tabs.executeScript(tab.id, {code: "document.title = 'New Message (1)'"});
  });
});

add_task(async function test_without_tabs_permission() {
  await do_test_update(async function background() {
    const url = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context_tabs_onUpdated_page.html";
    const tab = await browser.tabs.create({url});
    let count = 0;

    browser.tabs.onUpdated.addListener(function onUpdated(tabId, changeInfo) {
      browser.test.assertEq(tabId, tab.id, "Check tab id");
      browser.test.log(`onUpdated: ${JSON.stringify(changeInfo)}`);

      browser.test.assertFalse("url" in changeInfo, "url should not be included without tabs permission");
      browser.test.assertFalse("favIconUrl" in changeInfo, "favIconUrl should not be included without tabs permission");
      browser.test.assertFalse("title" in changeInfo, "title should not be included without tabs permission");

      if (changeInfo.status == "complete") {
        count++;
        if (count === 2) {
          browser.test.log("Reload complete");
          browser.tabs.onUpdated.removeListener(onUpdated);
          browser.tabs.remove(tabId);
          browser.test.notifyPass("finish");
        }
      }
    });

    browser.tabs.reload(tab.id);
  }, false /* withPermissions */);
});

add_task(forceGC);
