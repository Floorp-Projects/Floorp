"use strict";

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
    ],
  });
});

add_task(async function test_add_address() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let addresses = await getAddresses();

  is(addresses.length, 0, "No address in storage");

  await BrowserTestUtils.withNewTab(
    { gBrowser: privateWin.gBrowser, url: FORM_URL },
    async function(privateBrowser) {
      await focusUpdateSubmitForm(privateBrowser, {
        focusSelector: "#organization",
        newValues: {
          "#organization": "Mozilla",
          "#street-address": "331 E. Evelyn Avenue",
          "#tel": "1-650-903-0800",
        },
      });
    }
  );

  // Wait 1 second to make sure the profile has not been saved
  await new Promise(resolve =>
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    setTimeout(resolve, TIMEOUT_ENSURE_PROFILE_NOT_SAVED)
  );
  addresses = await getAddresses();
  is(addresses.length, 0, "No address saved in private browsing mode");

  await BrowserTestUtils.closeWindow(privateWin);
});
