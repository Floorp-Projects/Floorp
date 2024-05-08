/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * The tests should help understand what the web progress listeners
 * (in FormHandlerChild) are listening for. These examples are not a complete list.
 *
 * Changes to a window's location and history instances are handled as page navigations:
 *  - history.pushState(..)
 *  - history.replaceState(..)
 *  - location.href = ..
 *  - location.replace(..)
 * History navigations and location reloads are ignored:
 *  - history.go(-1)
 *  - history.back()
 *  - location.reload()
 */

"use strict";

const ADDRESS_VALUES = {
  "#given-name": "Test User",
  "#organization": "Sesame Street",
  "#street-address": "123 Sesame Street",
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.heuristics.captureOnPageNavigation", true],
    ],
  });
});

/**
 * Tests that the address is captured (address doorhanger is shown)
 * after pushing a history entry (history.pushState)
 */
add_task(async function test_address_captured_on_history_pushState() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info("Update identified address fields");
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
      );

      info("Navigate the page by pushing a history entry");
      await SpecialPowers.spawn(browser, [], () => {
        const historyPushStateBtn =
          content.document.getElementById("historyPushState");
        historyPushStateBtn.click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});

/**
 * Tests that the address is captured (address doorhanger is shown)
 * after replacing the current history entry (history.replaceState)
 */
add_task(async function test_address_captured_on_history_replaceState() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info("Update identified address fields");
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
      );

      info("Navigate the page by replacing the current history entry");
      await SpecialPowers.spawn(browser, [], () => {
        const historyReplaceStateBtn = content.document.getElementById(
          "historyReplaceState"
        );
        historyReplaceStateBtn.click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});

/**
 * Tests that the address is captured (address doorhanger is shown)
 * after navigating to another URL by changing location.href
 */
add_task(async function test_address_captured_on_location_href_change() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
      );

      info("Navigate the page by changing location.href");
      await SpecialPowers.spawn(browser, [], () => {
        const locationHrefBtn = content.document.getElementById("locationHref");
        locationHrefBtn.click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});

/**
 * Tests that the address is captured (address doorhanger is shown)
 * after navigating to another URL by replacing current location (location.replace)
 */
add_task(async function test_address_captured_on_location_replace() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
      );

      info("Navigate page by replacing current location");
      await SpecialPowers.spawn(browser, [], () => {
        const locationReplaceBtn =
          content.document.getElementById("locationReplace");
        locationReplaceBtn.click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});

/**
 * Tests that the address is captured (address doorhanger is shown)
 * after reloading the current location (location.reload)
 */
add_task(async function test_address_captured_on_location_reload() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
      );

      info("Reload the current location");
      await SpecialPowers.spawn(browser, [], () => {
        const locationReloadBtn =
          content.document.getElementById("locationReload");
        locationReloadBtn.click();
      });

      info("Ensure address doorhanger not shown");
      await ensureNoDoorhanger();

      ok(true, "Address doorhanger is not shown");
    }
  );
});

/**
 * Tests that the address isn't captured (address doorhanger isn't shown)
 * after going to specific page in the session history (history.go)
 */
add_task(async function test_address_captured_on_history_go() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
      );

      info("Go to specific page in session history");
      await SpecialPowers.spawn(browser, [], () => {
        const historyGoBtn = content.document.getElementById("historyGo");
        historyGoBtn.click();
      });

      info("Ensure address doorhanger not shown");
      await ensureNoDoorhanger();

      ok(true, "Address doorhanger is not shown");
    }
  );
});

/**
 * Tests that the address isn't captured (address doorhanger isn't shown)
 * after moving back in the session history (history.back)
 */
add_task(async function test_address_not_captured_on_history_back() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
      );

      info("Move back in the session history");
      await SpecialPowers.spawn(browser, [], () => {
        const historyBackBtn = content.document.getElementById("historyBack");
        historyBackBtn.click();
      });

      info("Ensure address doorhanger not shown");
      await ensureNoDoorhanger();

      ok(true, "Address doorhanger is not shown");
    }
  );
});
