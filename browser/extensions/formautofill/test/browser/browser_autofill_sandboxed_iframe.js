"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
    ],
  });
  await setStorage(TEST_ADDRESS_2, TEST_ADDRESS_3);
});

add_task(async function test_autocomplete_in_sandboxed_iframe() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: FORM_IFRAME_SANDBOXED_URL },
    async browser => {
      const iframeBC = browser.browsingContext.children[0];
      await openPopupOnSubframe(browser, iframeBC, "#street-address");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, iframeBC);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, iframeBC);
      await waitForAutofill(
        iframeBC,
        "#street-address",
        TEST_ADDRESS_2["street-address"]
      );
      Assert.ok(true, "autocomplete works in sandboxed iframe");
    }
  );

  await removeAllRecords();
});
