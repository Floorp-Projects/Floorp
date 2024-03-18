"use strict";

const URL = BASE_URL + "autocomplete_basic.html";

const l10n = new Localization(["toolkit/formautofill/formAutofill.ftl"], true);

add_setup(async function setup_storage() {
  await setStorage(
    TEST_ADDRESS_2,
    TEST_ADDRESS_3,
    TEST_ADDRESS_4,
    TEST_ADDRESS_5,
    TEST_CREDIT_CARD_1
  );
});

function getFooterLabel(itemsBox) {
  let footer = itemsBox.getItemAtIndex(itemsBox.itemCount - 1);
  while (footer.collapsed) {
    footer = footer.previousSibling;
  }

  return footer.querySelector(".line1-label");
}

add_task(async function test_footer_has_correct_button_text_on_address() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: URL },
    async function (browser) {
      const {
        autoCompletePopup: { richlistbox: itemsBox },
      } = browser;

      await openPopupOn(browser, "#organization");
      let footer = getFooterLabel(itemsBox);
      Assert.equal(
        footer.innerText,
        l10n.formatValueSync("autofill-manage-addresses-label")
      );
      await closePopup(browser);
    }
  );
});

add_task(async function test_footer_has_correct_button_text_on_credit_card() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      const {
        autoCompletePopup: { richlistbox: itemsBox },
      } = browser;

      await openPopupOn(browser, "#cc-number");
      let footer = getFooterLabel(itemsBox);
      Assert.equal(
        footer.innerText,
        l10n.formatValueSync("autofill-manage-payment-methods-label")
      );
      await closePopup(browser);
    }
  );
});

add_task(async function test_press_enter_on_footer() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: URL },
    async function (browser) {
      const {
        autoCompletePopup: { richlistbox: itemsBox },
      } = browser;

      await openPopupOn(browser, "#organization");

      // Navigate to the footer and press enter.
      const listItemElems = itemsBox.querySelectorAll(
        ".autocomplete-richlistitem"
      );
      const prefTabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        PRIVACY_PREF_URL,
        true
      );
      for (let i = 0; i < listItemElems.length; i++) {
        if (!listItemElems[i].disabled) {
          await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        }
      }
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      info(`expecting tab: about:preferences#privacy opened`);
      const prefTab = await prefTabPromise;
      info(`expecting tab: about:preferences#privacy removed`);
      BrowserTestUtils.removeTab(prefTab);
      ok(
        true,
        "Tab: preferences#privacy was successfully opened by pressing enter on the footer"
      );

      await closePopup(browser);
    }
  );
});

add_task(async function test_click_on_footer() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: URL },
    async function (browser) {
      const {
        autoCompletePopup: { richlistbox: itemsBox },
      } = browser;

      await openPopupOn(browser, "#organization");
      // Click on the footer
      let optionButton = itemsBox.querySelector(
        ".autocomplete-richlistitem:last-child"
      );
      while (optionButton.collapsed) {
        optionButton = optionButton.previousElementSibling;
      }

      const prefTabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        PRIVACY_PREF_URL,
        true
      );
      // Make sure dropdown is visible before continuing mouse synthesizing.
      await BrowserTestUtils.waitForCondition(() =>
        BrowserTestUtils.isVisible(optionButton)
      );
      await EventUtils.synthesizeMouseAtCenter(optionButton, {});
      info(`expecting tab: about:preferences#privacy opened`);
      const prefTab = await prefTabPromise;
      info(`expecting tab: about:preferences#privacy removed`);
      BrowserTestUtils.removeTab(prefTab);
      ok(
        true,
        "Tab: preferences#privacy was successfully opened by clicking on the footer"
      );

      await closePopup(browser);
    }
  );
});

add_task(async function test_phishing_warning_single_category() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: URL },
    async function (browser) {
      await openPopupOn(browser, "#tel");
      await expectWarningText(browser, "Also autofills address");
      await closePopup(browser);
    }
  );
});

add_task(async function test_phishing_warning_complex_categories() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: URL },
    async function (browser) {
      await openPopupOn(browser, "#street-address");

      await expectWarningText(browser, "Also autofills organization, email");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await expectWarningText(browser, "Autofills address");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await expectWarningText(browser, "Also autofills organization, email");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await expectWarningText(browser, "Also autofills organization, email");

      await closePopup(browser);
    }
  );
});
