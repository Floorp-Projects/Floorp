/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testSetup() {
  Services.prefs.setBoolPref("toolkit.cosmeticAnimations.enabled", false);
});

add_task(async function testPopupSelectPopup() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "https://example.com",
  });

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        browser.pageAction.show(tabs[0].id);
      });
    },

    manifest: {
      browser_action: {
        default_popup: "popup.html",
        browser_style: false,
      },

      page_action: {
        default_popup: "popup.html",
        browser_style: false,
      },
    },

    files: {
      "popup.html": `<!DOCTYPE html>
        <html>
          <head><meta charset="utf-8"></head>
          <body style="width: 300px; height: 300px;">
            <div style="text-align: center">
              <select id="select">
                <option>Foo</option>
                <option>Bar</option>
                <option>Baz</option>
              </select>
            </div>
          </body>
        </html>`,
    },
  });

  await extension.startup();

  let selectPopup = document.getElementById("ContentSelectDropdown")
    .firstElementChild;

  async function testPanel(browser) {
    let popupPromise = promisePopupShown(selectPopup);

    // Wait the select element in the popup window to be ready before sending a
    // mouse event to open the select popup.
    await ContentTask.spawn(browser, null, async () => {
      await ContentTaskUtils.waitForCondition(() => {
        return content.document && content.document.querySelector("#select");
      });
    });
    BrowserTestUtils.synthesizeMouseAtCenter("#select", {}, browser);

    await popupPromise;

    let elemRect = await ContentTask.spawn(browser, null, async function() {
      let elem = content.document.getElementById("select");
      let r = elem.getBoundingClientRect();

      return { left: r.left, bottom: r.bottom };
    });

    let popupRect = selectPopup.getOuterScreenRect();

    is(
      Math.floor(browser.screenX + elemRect.left),
      popupRect.left,
      "Select popup has the correct x origin"
    );

    is(
      Math.floor(browser.screenY + elemRect.bottom),
      popupRect.top,
      "Select popup has the correct y origin"
    );

    // Close the select popup before proceeding to the next test.
    const onPopupHidden = BrowserTestUtils.waitForEvent(
      selectPopup,
      "popuphidden"
    );
    selectPopup.hidePopup();
    await onPopupHidden;
  }

  {
    info("Test browserAction popup");

    clickBrowserAction(extension);
    let browser = await awaitExtensionPanel(extension);
    await testPanel(browser);
    await closeBrowserAction(extension);
  }

  {
    info("Test pageAction popup");

    clickPageAction(extension);
    let browser = await awaitExtensionPanel(extension);
    await testPanel(browser);
    await closePageAction(extension);
  }

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function testTeardown() {
  Services.prefs.clearUserPref("toolkit.cosmeticAnimations.enabled");
});
