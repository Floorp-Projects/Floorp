add_task(function* () {
  let win1 = yield BrowserTestUtils.openNewBrowserWindow();

  yield focusWindow(win1);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
      "content_scripts": [{
        "matches": ["http://mochi.test/*/context_tabs_onUpdated_page.html"],
        "js": ["content-script.js"],
        "run_at": "document_start"
      },],
    },

    background: function() {
      var pageURL = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context_tabs_onUpdated_page.html";

      var expectedSequence = [
        { status: "loading" },
        { status: "loading", url: pageURL },
        { status: "complete" }
      ];
      var collectedSequence = [];

      browser.tabs.onUpdated.addListener(function (tabId, updatedInfo) {
        collectedSequence.push(updatedInfo);
      });

      browser.runtime.onMessage.addListener(function () {
          if (collectedSequence.length !== expectedSequence.length) {
            browser.test.assertEq(
              JSON.stringify(expectedSequence),
              JSON.stringify(collectedSequence),
              "got unexpected number of updateInfo data"
            );
          } else {
            for (var i = 0; i < expectedSequence.length; i++) {
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

      browser.tabs.create({ url: pageURL });
    },
    files: {
      "content-script.js": `
        window.addEventListener("message", function(evt) {
          if (evt.data == "frame-updated") {
            browser.runtime.sendMessage("load-completed");
          }
        }, true);
      `,
    }
  });

  yield Promise.all([
    extension.startup(),
    extension.awaitFinish("tabs.onUpdated")
  ]);

  yield extension.unload();

  yield BrowserTestUtils.closeWindow(win1);
});
