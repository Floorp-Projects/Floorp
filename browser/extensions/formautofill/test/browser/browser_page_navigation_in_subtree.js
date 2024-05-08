/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This test file tests the form submission heuristic depending on detecting page navigations
 */

"use strict";

const ADDRESS_VALUES = {
  "#postal-code": "02139",
  "#organization": "Sesame Street",
  "#street-address": "123 Sesame Street",
  "#country": "US",
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.heuristics.captureOnPageNavigation", true],
      ["extensions.formautofill.addresses.capture.requiredFields", ""],
    ],
  });
});

/**
 * Tests that address fields are not captured (doorhanger is not shown)
 * when the page navigation happens in a same-origin iframe and
 * the address fields are in a the parent window
 */
add_task(
  async function test_parent_window_not_effected_when_same_origin_iframe_window_is_navigated() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: ADDRESS_FORM_URL,
      },
      async function (browser) {
        info("Update address fields in parent window");
        await focusUpdateSubmitForm(
          browser,
          {
            focusSelector: "#street-address",
            newValues: ADDRESS_VALUES,
          },
          false // We don't submit the form
        );

        info("Load same-origin iframe and infer page navigation");
        await SpecialPowers.spawn(browser, [], async () => {
          let doc = content.document;

          let iframe = doc.createElement("iframe");
          const iframeLoadPromise = new Promise(
            resolve => (iframe.onload = resolve)
          );
          // same-origin iframe
          iframe.setAttribute(
            "src",
            "https://example.org/browser/browser/extensions/formautofill/test/fixtures/page_navigation.html"
          );
          doc.body.appendChild(iframe);

          await iframeLoadPromise;

          // We can directly access the button because the iframe is of same origin
          iframe.contentDocument.getElementById("windowLocationBtn").click();
        });

        info("Ensure address doorhanger not shown");
        await ensureNoDoorhanger();
      }
    );
  }
);

/**
 * Tests that address fields are not captured (doorhanger is not shown)
 * when the page navigation happens in a cross-origin iframe and
 * the address fields are in a the parent window
 */
add_task(
  async function test_parent_window_not_effected_when_cross_origin_iframe_window_is_navigated() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: ADDRESS_FORM_URL,
      },
      async function (browser) {
        info("Update address fields in parent window");
        await focusUpdateSubmitForm(
          browser,
          {
            focusSelector: "#street-address",
            newValues: ADDRESS_VALUES,
          },
          false // We don't submit the form
        );

        info("Load cross-origin iframe");
        await SpecialPowers.spawn(browser, [], async () => {
          let doc = content.document;

          let iframe = doc.createElement("iframe");
          const iframeLoadPromise = new Promise(
            resolve => (iframe.onload = resolve)
          );
          // cross-origin iframe
          iframe.setAttribute(
            "src",
            "https://example.com/browser/browser/extensions/formautofill/test/fixtures/page_navigation.html"
          );
          doc.body.appendChild(iframe);

          await iframeLoadPromise;
        });

        let iframeBC = browser.browsingContext.children[0];

        info("Infer page navigation in cross-origin iframe window");
        await SpecialPowers.spawn(iframeBC, [], async () => {
          content.document.getElementById("windowLocationBtn").click();
        });

        info("Ensure address doorhanger not shown");
        await ensureNoDoorhanger();
      }
    );
  }
);

/**
 * Tests that address fields are captured (doorhanger is shown)
 * when address fields are in a same-origin iframe and
 * the parent window is navigated
 */
add_task(
  async function test_same_origin_window_is_effected_when_parent_window_is_navigated() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "https://example.org/browser/browser/extensions/formautofill/test/fixtures/page_navigation.html",
      },
      async function (browser) {
        const onPopupShown = waitForPopupShown();

        info("Load same-origin iframe with address form");
        await SpecialPowers.spawn(browser, [], async () => {
          let doc = content.document;

          let iframe = doc.createElement("iframe");
          const iframeLoadPromise = new Promise(
            resolve => (iframe.onload = resolve)
          );
          // same-origin iframe
          iframe.setAttribute(
            "src",
            "https://example.org/browser/browser/extensions/formautofill/test/fixtures/autocomplete_address_basic.html"
          );
          doc.body.appendChild(iframe);

          await iframeLoadPromise;
        });

        let iframeBC = browser.browsingContext.children[0];

        info("Update address fields in same-origin iframe");
        await focusUpdateSubmitForm(
          iframeBC,
          {
            focusSelector: "#street-address",
            newValues: ADDRESS_VALUES,
          },
          false // We don't submit the form
        );

        info("Infer page navigation in parent window");
        await SpecialPowers.spawn(browser, [], async () => {
          content.document.getElementById("windowLocationBtn").click();
        });

        info("Wait for address doorhanger");
        await onPopupShown;

        ok(true, "Address doorhanger is shown");
      }
    );
  }
);

/**
 * Tests that address fields are captured (doorhanger is shown)
 * when address fields are in a cross-origin iframe and
 * the parent window is navigated
 */
add_task(
  async function test_cross_origin_window_is_effected_when_parent_window_is_navigated() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "https://example.org/browser/browser/extensions/formautofill/test/fixtures/page_navigation.html",
      },
      async function (browser) {
        const onPopupShown = waitForPopupShown();

        info("Load cross-origin iframe with address form");
        await SpecialPowers.spawn(browser, [], async () => {
          let doc = content.document;

          let iframe = doc.createElement("iframe");
          const iframeLoadPromise = new Promise(
            resolve => (iframe.onload = resolve)
          );
          // cross-origin iframe
          iframe.setAttribute(
            "src",
            "https://example.com/browser/browser/extensions/formautofill/test/fixtures/autocomplete_address_basic.html"
          );
          doc.body.appendChild(iframe);

          await iframeLoadPromise;
        });

        let iframeBC = browser.browsingContext.children[0];

        info("Update address fields in cross-origin iframe");
        await focusUpdateSubmitForm(
          iframeBC,
          {
            focusSelector: "#street-address",
            newValues: ADDRESS_VALUES,
          },
          false // We don't submit the form
        );

        info("Infer page navigation in parent window");
        await SpecialPowers.spawn(browser, [], async () => {
          content.document.getElementById("windowLocationBtn").click();
        });

        info("Wait for address doorhanger");
        await onPopupShown;

        ok(true, "Address doorhanger is shown");
      }
    );
  }
);

/**
 * Tests that address fields are captured (doorhanger is shown)
 * when address fields are in a same-origin iframe and
 * the iframe's window is navigated
 */
add_task(async function test_same_origin_iframe_window_is_navigated() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: EMPTY_URL,
    },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info(
        "Load same-origin iframe with address fields and page navigation button"
      );
      await SpecialPowers.spawn(browser, [], async () => {
        let doc = content.document;

        let iframe = doc.createElement("iframe");
        const iframeLoadPromise = new Promise(
          resolve => (iframe.onload = resolve)
        );
        // same-origin iframe
        iframe.setAttribute(
          "src",
          "https://example.org/browser/browser/extensions/formautofill/test/fixtures/capture_address_on_page_navigation.html"
        );
        doc.body.appendChild(iframe);

        await iframeLoadPromise;
      });

      let iframeBC = browser.browsingContext.children[0];

      info("Update address fields in same-origin iframe");
      await focusUpdateSubmitForm(
        iframeBC,
        {
          focusSelector: "#street-address",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
      );

      info("Infer page navigation in same-origin iframe's window");
      await SpecialPowers.spawn(iframeBC, [], async () => {
        content.document.getElementById("locationHref").click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});

// /**
//  * Tests that address fields are captured (doorhanger is shown)
//  * when address fields are in a cross-origin iframe and
//  * the iframe's window is navigated
//  */
add_task(async function test_cross_origin_iframe_window_is_navigated() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: EMPTY_URL,
    },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info(
        "Load same-origin iframe with address fields and page navigation button"
      );
      await SpecialPowers.spawn(browser, [], async () => {
        let doc = content.document;

        let iframe = doc.createElement("iframe");
        const iframeLoadPromise = new Promise(
          resolve => (iframe.onload = resolve)
        );
        // cross-origin iframe
        iframe.setAttribute(
          "src",
          "https://example.com/browser/browser/extensions/formautofill/test/fixtures/capture_address_on_page_navigation.html"
        );
        doc.body.appendChild(iframe);

        await iframeLoadPromise;
      });

      let iframeBC = browser.browsingContext.children[0];

      info("Update address fields in cross-origin iframe");
      await focusUpdateSubmitForm(
        iframeBC,
        {
          focusSelector: "#street-address",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
      );

      info("Infer page navigation in cross-origin iframe's window");
      await SpecialPowers.spawn(iframeBC, [], async () => {
        content.document.getElementById("locationHref").click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});

/**
 * Tests that address fields are captured (doorhanger is shown)
 * when we have the three following nested frames:
 *   - top level frame
 *   - second level same-origin iframe being navigated
 *   - third level same-origin iframe with address fields
 *
 * Test also that the we don't instantiate a FormHandler actor pair in the second level window.
 * This is to make sure that we don't set up more instances then necessary
 */
add_task(
  async function test_third_level_same_origin_window_is_effected_when_second_level_same_origin_window_is_navigated() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "https://example.org/browser/browser/extensions/formautofill/test/browser/empty.html",
      },
      async function (browser) {
        const onPopupShown = waitForPopupShown();

        info("Load second level same-origin iframe with navigation button");
        await SpecialPowers.spawn(browser, [], async () => {
          let doc = content.document;

          let iframe = doc.createElement("iframe");
          const iframeLoadPromise = new Promise(
            resolve => (iframe.onload = resolve)
          );
          iframe.setAttribute(
            "src",
            "https://example.org/browser/browser/extensions/formautofill/test/fixtures/page_navigation.html"
          );
          doc.body.appendChild(iframe);

          await iframeLoadPromise;
        });

        let secondIframeBC = browser.browsingContext.children[0];

        info("Load third level same-origin iframe with address fields");
        await SpecialPowers.spawn(secondIframeBC, [], async () => {
          let doc = content.document;

          let iframe = doc.createElement("iframe");
          const iframeLoadPromise = new Promise(
            resolve => (iframe.onload = resolve)
          );
          iframe.setAttribute(
            "src",
            "https://example.org/browser/browser/extensions/formautofill/test/fixtures/autocomplete_address_basic.html"
          );
          doc.body.appendChild(iframe);

          await iframeLoadPromise;
        });

        let thirdIframeBC = secondIframeBC.children[0];

        info("Update address fields in third level iframe");
        await focusUpdateSubmitForm(
          thirdIframeBC,
          {
            focusSelector: "#street-address",
            newValues: ADDRESS_VALUES,
          },
          false // We don't submit the form
        );

        info(
          "Infer page navigation in second level same-origin iframe's window"
        );
        await SpecialPowers.spawn(secondIframeBC, [], async () => {
          content.document.getElementById("windowLocationBtn").click();
        });

        info("Wait for address doorhanger");
        await onPopupShown;

        ok(true, "Address doorhanger is shown");

        info(
          "Check that the FormHandler actor pair isn't instantiated unnecessarily in the second level window"
        );
        await SpecialPowers.spawn(secondIframeBC, [], async () => {
          is(
            content.windowGlobalChild.getExistingActor("FormHandler"),
            null,
            "FormHandlerChild doesn't exist in second level window"
          );
        });
        is(
          secondIframeBC.currentWindowGlobal.getExistingActor("FormHandler"),
          null,
          "FormHandlerParent doesn't exist in second level window"
        );
      }
    );
  }
);

/**
 * Tests that address fields are captured (doorhanger is shown)
 * when we have the three following nested frames:
 *   - top level frame
 *   - second level cross-origin iframe being navigated
 *   - third level cross-origin iframe with address fields
 *
 * Test also that the we also instantiate a FormHandler actor pair in the top window.
 * This way also a top navigation would have trigger the address capturing.
 */
add_task(
  async function test_third_level_cross_origin_window_is_effected_when_second_level_cross_origin_window_is_navigated() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "https://example.org/browser/browser/extensions/formautofill/test/browser/empty.html",
      },
      async function (browser) {
        const onPopupShown = waitForPopupShown();

        info("Load second level cross-origin iframe with navigation button");
        await SpecialPowers.spawn(browser, [], async () => {
          let doc = content.document;

          let iframe = doc.createElement("iframe");
          const iframeLoadPromise = new Promise(
            resolve => (iframe.onload = resolve)
          );
          iframe.setAttribute(
            "src",
            "https://example.com/browser/browser/extensions/formautofill/test/fixtures/page_navigation.html"
          );
          doc.body.appendChild(iframe);

          await iframeLoadPromise;
        });

        let secondIframeBC = browser.browsingContext.children[0];

        info("Load third level cross-origin iframe with address fields");
        await SpecialPowers.spawn(secondIframeBC, [], async () => {
          let doc = content.document;

          let iframe = doc.createElement("iframe");
          const iframeLoadPromise = new Promise(
            resolve => (iframe.onload = resolve)
          );
          iframe.setAttribute(
            "src",
            "https://example.com/browser/browser/extensions/formautofill/test/fixtures/autocomplete_address_basic.html"
          );
          doc.body.appendChild(iframe);

          await iframeLoadPromise;
        });

        let thirdIframeBC = secondIframeBC.children[0];

        info("Update address fields in third level iframe");
        await focusUpdateSubmitForm(
          thirdIframeBC,
          {
            focusSelector: "#street-address",
            newValues: ADDRESS_VALUES,
          },
          false // We don't submit the form
        );

        info(
          "Check that the FormHandler actor pair is instantiated in the top window"
        );
        await SpecialPowers.spawn(browser, [], async () => {
          isnot(
            content.windowGlobalChild.getExistingActor("FormHandler"),
            null,
            "FormHandlerChild exists in top window"
          );
        });
        isnot(
          browser.browsingContext.currentWindowGlobal.getExistingActor(
            "FormHandler"
          ),
          null,
          "FormHandlerParent exists in top window"
        );

        info(
          "Check that only the FormHandlerChild is instantiated in the second level window (parent will be created on navigation)"
        );
        await SpecialPowers.spawn(secondIframeBC, [], async () => {
          isnot(
            content.windowGlobalChild.getExistingActor("FormHandler"),
            null,
            "FormHandlerChild exists in second level window"
          );
        });
        is(
          secondIframeBC.currentWindowGlobal.getExistingActor("FormHandler"),
          null,
          "FormHandlerParent doesn't exist in second level window"
        );

        info(
          "Infer page navigation in second level cross-origin iframe's window"
        );
        await SpecialPowers.spawn(secondIframeBC, [], async () => {
          content.document.getElementById("windowLocationBtn").click();
        });

        info("Wait for address doorhanger");
        await onPopupShown;

        ok(true, "Address doorhanger is shown");
      }
    );
  }
);
