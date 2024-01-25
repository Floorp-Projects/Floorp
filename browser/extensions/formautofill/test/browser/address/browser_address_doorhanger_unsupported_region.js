"use strict";

const { Region } = ChromeUtils.importESModule(
  "resource://gre/modules/Region.sys.mjs"
);
add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "detect"],
      ["extensions.formautofill.addresses.supportedCountries", "US,CA"],
    ],
  });
});

add_task(async function test_save_doorhanger_supported_region() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": "John",
          "#family-name": "Doe",
          "#organization": "Mozilla",
          "#street-address": "123 Sesame Street",
          "#country": "US",
        },
      });
      await onPopupShown;
    }
  );
});

/**
 * Do not display the address capture doorhanger if the country field of the
 * submitted form is not on the supported list."
 */
add_task(async function test_save_doorhanger_unsupported_region_from_record() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": "John",
          "#family-name": "Doe",
          "#organization": "Mozilla",
          "#street-address": "123 Sesame Street",
          "#country": "DE",
        },
      });

      await ensureNoDoorhanger(browser);
    }
  );
});

add_task(async function test_save_doorhanger_unsupported_region_from_pref() {
  const initialHomeRegion = Region._home;
  const initialCurrentRegion = Region._current;

  const region = "FR";
  Region._setCurrentRegion(region);
  Region._setHomeRegion(region);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": "John",
          "#family-name": "Doe",
          "#organization": "Mozilla",
          "#street-address": "123 Sesame Street",
        },
      });

      await ensureNoDoorhanger(browser);
    }
  );

  Region._setCurrentRegion(initialHomeRegion);
  Region._setHomeRegion(initialCurrentRegion);
});
