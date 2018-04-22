/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

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
        org.setUserInput("Mozilla");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
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
        tel.setUserInput("+1234567890");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "2 addresses in storage");
  is(addresses[1].tel, "+1234567890", "Verify the tel field");
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
        tel.setUserInput("+16172535702");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "Still 2 addresses in storage");
});

add_task(async function test_submit_untouched_fields() {
  let addresses = await getAddresses();
  is(addresses.length, 2, "2 addresses in storage");

  await BrowserTestUtils.withNewTab({gBrowser, url: FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await openPopupOn(browser, "form #organization");
      info("before down");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      info("after down, before return");
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      info("after return");

      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let org = form.querySelector("#organization");
        await new Promise(resolve => setTimeout(resolve, 1000));
        org.setUserInput("Organization");

        let tel = form.querySelector("#tel");
        await new Promise(resolve => setTimeout(resolve, 1000));
        tel.value = "12345"; // ".value" won't change the highlight status.

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        info("before submit");
        form.querySelector("input[type=submit]").click();
        info("after submit");
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "Still 2 addresses in storage");
  is(addresses[0].organization, "Organization", "organization should change");
  is(addresses[0].tel, "+16172535702", "tel should remain unchanged");
});

add_task(async function test_submit_reduced_fields() {
  let addresses = await getAddresses();
  is(addresses.length, 2, "2 addresses in storage");

  let url = BASE_URL + "autocomplete_simple_basic.html";
  await BrowserTestUtils.withNewTab({gBrowser, url},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await openPopupOn(browser, "form#simple input[name=tel]");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);

      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.querySelector("form#simple");
        let tel = form.querySelector("input[name=tel]");
        await new Promise(resolve => setTimeout(resolve, 1000));
        tel.setUserInput("123456789");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "Still 2 addresses in storage");
  is(addresses[0].tel, "123456789", "tel should should be changed");
  is(addresses[0]["postal-code"], "02139", "postal code should be kept");
});
