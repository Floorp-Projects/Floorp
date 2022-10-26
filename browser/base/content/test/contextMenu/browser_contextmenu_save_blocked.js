/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

function mockPromptService() {
  let { prompt } = Services;
  let promptService = {
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
    alert: () => {},
  };
  Services.prompt = promptService;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });
  return promptService;
}

add_task(async function test_save_link_blocked_by_extension() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: "cancel@test" } },
      name: "Cancel Test",
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },

    background() {
      // eslint-disable-next-line no-undef
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          return { cancel: details.url === "http://example.com/" };
        },
        { urls: ["*://*/*"] },
        ["blocking"]
      );
    },
  });
  await ext.startup();

  await BrowserTestUtils.withNewTab(
    `data:text/html;charset=utf-8,<a href="http://example.com">Download</a>`,
    async browser => {
      let menu = document.getElementById("contentAreaContextMenu");
      let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      BrowserTestUtils.synthesizeMouseAtCenter(
        "a[href]",
        { type: "contextmenu", button: 2 },
        browser
      );
      await popupShown;

      await new Promise(resolve => {
        let promptService = mockPromptService();
        promptService.alert = (window, title, msg) => {
          is(
            msg,
            "The download cannot be saved because it is blocked by Cancel Test.",
            "prompt should be shown"
          );
          setTimeout(resolve, 0);
        };

        MockFilePicker.showCallback = function(fp) {
          ok(false, "filepicker should never been shown");
          setTimeout(resolve, 0);
          return Ci.nsIFilePicker.returnCancel;
        };
        menu.activateItem(menu.querySelector("#context-savelink"));
      });
    }
  );

  await ext.unload();
});
