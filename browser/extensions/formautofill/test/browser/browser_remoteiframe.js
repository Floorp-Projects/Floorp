/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const IFRAME_URL_PATH = BASE_URL + "autocomplete_iframe.html";
const PRIVACY_PREF_URL = "about:preferences#privacy";

// Start by adding a few addresses to storage.
add_task(async function setup_storage() {
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_4);
  await saveAddress(TEST_ADDRESS_5);
});

// Verify that form fillin works in a remote iframe, and that changing
// a field updates storage.
add_task(async function test_iframe_autocomplete() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    IFRAME_URL_PATH,
    true
  );
  let browser = tab.linkedBrowser;
  let iframeBC = browser.browsingContext.getChildren()[1];
  await openPopupForSubframe(browser, iframeBC, "#street-address");

  // Highlight the first item in the list. We want to verify
  // that the warning text is correct to ensure that the preview is
  // performed properly.

  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, iframeBC);
  await expectWarningText(browser, "Autofills address");

  // Highlight and select the second item in the list
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, iframeBC);
  await expectWarningText(browser, "Also autofills organization, email");
  EventUtils.synthesizeKey("VK_RETURN", {});

  let promiseShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  await new Promise(resolve => setTimeout(resolve, 1000));

  let loadPromise = BrowserTestUtils.browserLoaded(browser, true);
  await SpecialPowers.spawn(iframeBC, [], async function() {
    Assert.equal(
      content.document.getElementById("street-address").value,
      "32 Vassar Street MIT Room 32-G524"
    );
    Assert.equal(content.document.getElementById("country").value, "US");

    let org = content.document.getElementById("organization");
    Assert.equal(org.value, "World Wide Web Consortium");

    // Now, modify the organization.
    org.setUserInput("Example Inc.");

    await new Promise(resolve => content.setTimeout(resolve, 1000));
    content.document.querySelector("input[type=submit]").click();
  });

  await loadPromise;
  await promiseShown;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed");
  await clickDoorhangerButton(MAIN_BUTTON);
  await onChanged;

  // Check that the organization was updated properly.
  let addresses = await getAddresses();
  is(addresses.length, 3, "Still 1 address in storage");
  is(
    addresses[1].organization,
    "Example Inc.",
    "Verify the organization field"
  );

  // Fill in the details again and then clear the form from the dropdown.
  await openPopupForSubframe(browser, iframeBC, "#street-address");
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, iframeBC);
  EventUtils.synthesizeKey("VK_RETURN", {});

  await new Promise(resolve => setTimeout(resolve, 1000));

  // Open the dropdown and select the Clear Form item.
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, iframeBC);
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, iframeBC);
  EventUtils.synthesizeKey("VK_RETURN", {});

  await new Promise(resolve => setTimeout(resolve, 1000));

  await SpecialPowers.spawn(iframeBC, [], async function() {
    Assert.equal(content.document.getElementById("street-address").value, "");
    Assert.equal(content.document.getElementById("country").value, "");
    Assert.equal(content.document.getElementById("organization").value, "");
  });

  await BrowserTestUtils.removeTab(tab);
});

// Choose preferences from the autocomplete dropdown within an iframe.
add_task(async function test_iframe_autocomplete_preferences() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    IFRAME_URL_PATH,
    true
  );
  let browser = tab.linkedBrowser;
  let iframeBC = browser.browsingContext.getChildren()[1];
  await openPopupForSubframe(browser, iframeBC, "#organization");

  await expectWarningText(browser, "Also autofills address, email");

  const prefTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    PRIVACY_PREF_URL
  );

  // Select the preferences item.
  EventUtils.synthesizeKey("VK_DOWN", {});
  EventUtils.synthesizeKey("VK_DOWN", {});
  EventUtils.synthesizeKey("VK_RETURN", {});

  info(`expecting tab: about:preferences#privacy opened`);
  const prefTab = await prefTabPromise;
  info(`expecting tab: about:preferences#privacy removed`);
  BrowserTestUtils.removeTab(prefTab);

  await BrowserTestUtils.removeTab(tab);
});
