"use strict";

const FORM_URL = "http://mochi.test:8888/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";

add_task(async function test_update_address() {
  await saveAddress(TEST_ADDRESS_1);
  let addresses = await getAddresses();
  is(addresses.length, 1, "1 address in storage");

  await BrowserTestUtils.withNewTab({gBrowser, url: FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await openPopupOn(browser, "form #organization");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);

      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let org = form.querySelector("#organization");
        await new Promise(resolve => setTimeout(resolve, 1000));
        org.value = "Mozilla";

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON_INDEX);
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 1, "Still 1 address in storage");
  is(addresses[0].organization, "Mozilla", "Verify the organization field");
});

add_task(async function test_create_new_address() {
  let addresses = await getAddresses();
  is(addresses.length, 1, "1 address in storage");

  await BrowserTestUtils.withNewTab({gBrowser, url: FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await openPopupOn(browser, "form #tel");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);

      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let tel = form.querySelector("#tel");
        await new Promise(resolve => setTimeout(resolve, 1000));
        tel.value = "+1-234-567-890";

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(SECONDARY_BUTTON_INDEX);
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "2 addresses in storage");
  is(addresses[1].tel, "+1-234-567-890", "Verify the tel field");
});

add_task(async function test_create_new_address_merge() {
  let addresses = await getAddresses();
  is(addresses.length, 2, "2 addresses in storage");

  await BrowserTestUtils.withNewTab({gBrowser, url: FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await openPopupOn(browser, "form #tel");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);

      // Choose the latest address and revert to the original phone number
      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let tel = form.querySelector("#tel");
        tel.value = "+1 617 253 5702";

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(SECONDARY_BUTTON_INDEX);
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "Still 2 addresses in storage");
});
