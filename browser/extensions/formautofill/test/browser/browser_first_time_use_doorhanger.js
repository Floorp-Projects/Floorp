"use strict";

const FORM_URL = "http://mochi.test:8888/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";
const FTU_PREF = "extensions.formautofill.firstTimeUse";
const ENABLED_PREF = "extensions.formautofill.addresses.enabled";

registerCleanupFunction(async function() {
  let addresses = await getAddresses();
  if (addresses.length) {
    await removeAddresses(addresses.map(address => address.guid));
  }
});

add_task(async function test_first_time_save() {
  let addresses = await getAddresses();
  is(addresses.length, 0, "No profile in storage");
  await SpecialPowers.pushPrefEnv({
    "set": [
      [FTU_PREF, true],
      [ENABLED_PREF, true],
    ],
  });

  await BrowserTestUtils.withNewTab({gBrowser, url: FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#privacy");
      await ContentTask.spawn(browser, null, async function() {
        content.document.getElementById("organization").focus();
        content.document.getElementById("organization").value = "Sesame Street";
        content.document.getElementById("street-address").value = "123 Sesame Street";
        content.document.getElementById("tel").value = "1-345-345-3456";

        content.document.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      let notificationElement = PopupNotifications.panel.firstChild;
      // Open the panel via link
      let link = notificationElement.querySelector("popupnotificationcontent label");
      link.click();
      let tab = await tabPromise;
      ok(tab, "Privacy panel opened");
      await BrowserTestUtils.removeTab(tab);
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 1, "Profile saved");
  let ftuPref = SpecialPowers.getBoolPref(FTU_PREF);
  is(ftuPref, false, "First time use flag is false");
});

add_task(async function test_non_first_time_save() {
  let addresses = await getAddresses();
  let ftuPref = SpecialPowers.getBoolPref(FTU_PREF);
  is(ftuPref, false, "First time use flag is false");
  is(addresses.length, 1, "1 address in storage");

  await BrowserTestUtils.withNewTab({gBrowser, url: FORM_URL},
    async function(browser) {
      await ContentTask.spawn(browser, null, async function() {
        content.document.getElementById("organization").focus();
        content.document.getElementById("organization").value = "Mozilla";
        content.document.getElementById("street-address").value = "331 E. Evelyn Avenue";
        content.document.getElementById("tel").value = "1-650-903-0800";

        content.document.querySelector("input[type=submit]").click();
      });

      await new Promise(resolve => setTimeout(resolve, 1000));
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "Another address saved");
});
