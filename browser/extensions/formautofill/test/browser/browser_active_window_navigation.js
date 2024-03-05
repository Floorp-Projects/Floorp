/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const ADDRESS_VALUES = {
  "#postal-code": "02139",
  "#organization": "Sesame Street",
  "#street-address": "123 Sesame Street",
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.heuristics.captureOnPageNavigation", true],
      [
        "extensions.formautofill.addresses.capture.requiredFields",
        "street-address,postal-code,organization",
      ],
    ],
  });
});

/**
 * Tests that formautofill fields are not captured (doorhanger is not shown)
 * when the page navigation happens in a same-origin iframe and
 * the active window is the parent window
 */
add_task(
  async function test_active_parent_window_not_effected_when_same_origin_iframe_window_is_navigated() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: ADDRESS_FORM_URL,
      },
      async function (browser) {
        info("Update address fields");
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
 * Tests that formautofill fields are not captured (doorhanger is not shown)
 * when the page navigation happens in a cross-origin iframe and
 * the active window is the parent window
 */
add_task(
  async function test_active_parent_window_not_effected_when_cross_origin_iframe_window_is_navigated() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: ADDRESS_FORM_URL,
      },
      async function (browser) {
        info("Update address fields");
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
 * Tests that formautofill fields are captured (doorhanger is shown)
 * when active formautofill fields are in a same-origin iframe and
 * the parent window is navigated
 */
add_task(
  async function test_active_same_origin_window_is_effected_when_parent_window_is_navigated() {
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

        info("Update address fields");
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
 * Tests that formautofill fields are not captured (doorhanger is not shown)
 * when the active formautofill fields are in a cross-origin iframe and
 * the parent window is navigated
 */
add_task(
  async function test_active_cross_origin_window_not_effected_when_parent_window_is_navigated() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "https://example.org/browser/browser/extensions/formautofill/test/fixtures/page_navigation.html",
      },
      async function (browser) {
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

        info("Update address fields");
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

        info("Ensure address doorhanger not shown");
        await ensureNoDoorhanger();

        ok(true, "Address doorhanger is not shown");
      }
    );
  }
);

/**
 * Tests that formautofill fields are captured (doorhanger is shown)
 * when active formautofill fields are in a same-origin iframe and
 * the iframe's window is navigated
 */
add_task(async function test_active_same_origin_iframe_window_is_navigated() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: EMPTY_URL,
    },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info(
        "Load same-origin iframe with formautofill fields and page navigation button"
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

      info("Update address fields");
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
        content.document.getElementById("windowLocation").click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});

// /**
//  * Tests that formautofill fields are captured (doorhanger is shown)
//  * when active formautofill fields are in a cross-origin iframe and
//  * the iframe's window is navigated
//  */
add_task(async function test_active_cross_origin_iframe_window_is_navigated() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: EMPTY_URL,
    },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info(
        "Load same-origin iframe with formautofill fields and page navigation button"
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

      info("Update address fields");
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
        content.document.getElementById("windowLocation").click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});
