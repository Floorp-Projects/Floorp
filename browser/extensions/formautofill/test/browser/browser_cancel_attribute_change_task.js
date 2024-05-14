"use strict";

add_setup(async function setup_storage() {
  await setStorage(TEST_ADDRESS_1);
});

// Ensure the AC popup opens under the correct element (the one we focused) and
// not the one where an attribute change happend last.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1849198
add_task(
  async function test_autocomplete_popup_is_displayed_under_focused_element() {
    const url =
      "https://example.org/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";

    await BrowserTestUtils.withNewTab(
      { gBrowser, url },
      async function (browser) {
        await SpecialPowers.spawn(browser, [], async function () {
          // register on change handler which reacts on a different autofilled
          // field than the focused one
          const tel = content.document.getElementById("tel");
          tel.onchange = () => tel.setAttribute("autocomplete", "new-password");

          // place focus inside address level 1 field
          const addressLevel1 =
            content.document.getElementById("address-level1");
          addressLevel1.focus();
        });

        // open AC popup by pressing arrow down key
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        // wait for AC popup to open
        await BrowserTestUtils.waitForCondition(() => {
          return browser.autoCompletePopup.popupOpen;
        });
        // select first address by pressing arrow down key
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        // activate autofill by pressing enter
        await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);

        // place focus inside address level 1 field again
        await SpecialPowers.spawn(browser, [], async function () {
          const addressLevel1 =
            content.document.getElementById("address-level1");
          addressLevel1.focus();
        });

        // open AC popup again by pressing arrow down key
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        // wait for AC popup to open again
        await BrowserTestUtils.waitForCondition(() => {
          return browser.autoCompletePopup.popupOpen;
        });

        // ensure the popup opened on the correct element
        await SpecialPowers.spawn(browser, [], async function () {
          const formFillController = Cc[
            "@mozilla.org/satchel/form-fill-controller;1"
          ].getService(Ci.nsIFormFillController);
          Assert.equal(
            formFillController.focusedInput?.id,
            "address-level1",
            "formFillController has correct focusedInput"
          );
        });
      }
    );
  }
);
