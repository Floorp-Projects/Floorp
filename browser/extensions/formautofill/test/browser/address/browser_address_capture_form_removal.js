"use strict";

async function expectSavedAddresses(expectedAddresses) {
  const addresses = await getAddresses();
  is(
    addresses.length,
    expectedAddresses.length,
    `${addresses.length} address in the storage`
  );

  for (let i = 0; i < expectedAddresses.length; i++) {
    for (const [key, value] of Object.entries(expectedAddresses[i])) {
      is(addresses[i][key] ?? "", value, `field ${key} should be equal`);
    }
  }
  return addresses;
}

const ADDRESS_FIELD_VALUES = {
  "given-name": "John",
  organization: "Sesame Street",
  "street-address": "123 Sesame Street",
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.heuristics.captureOnFormRemoval", true],
    ],
  });
  await removeAllRecords();
});

/**
 * Tests if the address is captured (address doorhanger is shown) after a
 * successful xhr or fetch request followed by a form removal and
 * that the stored address record has the right values.
 */
add_task(async function test_address_captured_after_form_removal() {
  const onStorageChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: {
            "#given-name": ADDRESS_FIELD_VALUES["given-name"],
            "#organization": ADDRESS_FIELD_VALUES.organization,
            "#street-address": ADDRESS_FIELD_VALUES["street-address"],
          },
        },
        false
      );

      info("Infer a successfull fetch request");
      await SpecialPowers.spawn(browser, [], async () => {
        await content.fetch(
          "https://example.org/browser/browser/extensions/formautofill/test/browser/empty.html"
        );
      });

      info("Infer form removal");
      await SpecialPowers.spawn(browser, [], async function () {
        let form = content.document.getElementById("form");
        form.remove();
      });
      info("Wait for address doorhanger");
      await onPopupShown;

      info("Click Save in address doorhanger");
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    }
  );

  info("Wait for the address to be added to the storage.");
  await onStorageChanged;

  info("Ensure that address record was captured and saved correctly.");
  await expectSavedAddresses([ADDRESS_FIELD_VALUES]);

  await removeAllRecords();
});

/**
 * Tests that the address is not captured without a prior fetch or xhr request event
 */
add_task(async function test_address_not_captured_without_prior_fetch() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: {
            "#given-name": ADDRESS_FIELD_VALUES["given-name"],
            "#organization": ADDRESS_FIELD_VALUES.organization,
            "#street-address": ADDRESS_FIELD_VALUES["street-address"],
          },
        },
        false
      );

      info("Infer form removal");
      await SpecialPowers.spawn(browser, [], async function () {
        let form = content.document.getElementById("form");
        form.remove();
      });

      info("Ensure that address doorhanger is not shown");
      await ensureNoDoorhanger();
    }
  );
});

/**
 * Tests that the FormHandler only listens for events related to form removal (and dispatches its own custom events related to form removal) \
 * when an actor registered their interest in form removals with the FormHandler. This is to avoid listening to form removal events unnecessarily.
 */
add_task(
  async function test_form_removals_events_dispatched_only_when_actor_interested_in_form_removals() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "https://example.org/browser/browser/extensions/formautofill/test/fixtures/two_address_forms.html",
      },
      async function (browser) {
        info("Update identified address fields");
        // We don't submit the form
        await focusUpdateSubmitForm(
          browser,
          {
            formId: "form-filled",
            focusSelector: "#given-name",
            newValues: {
              "#given-name": ADDRESS_FIELD_VALUES["given-name"],
              "#organization": ADDRESS_FIELD_VALUES.organization,
              "#street-address": ADDRESS_FIELD_VALUES["street-address"],
            },
          },
          false
        );

        const domDocFetchSuccessPromise = BrowserTestUtils.waitForContentEvent(
          browser,
          "DOMDocFetchSuccess"
        );

        const beforeFormSubmissionPromise =
          BrowserTestUtils.waitForContentEvent(
            browser,
            "before-form-submission"
          );

        info("Infer a successfull fetch request");
        await SpecialPowers.spawn(browser, [], async () => {
          await content.fetch(
            "https://example.org/browser/browser/extensions/formautofill/test/browser/empty.html"
          );
        });

        // FormAutofill has registered an interest in form removals, so the FormHandler should have set a flag on
        // the document to enable the doc to dispatch the event DOMDocFetchSuccess to notify of xhr/fetch requests
        info("Wait for DOMDocFetchSuccess event");
        await domDocFetchSuccessPromise;

        // The FormHandler should dispatch the event before-form-submission when being notified of a DOMDocFetchSuccess event
        info("Wait for before-form-submission event");
        await beforeFormSubmissionPromise;

        const domFormRemovedPromise = BrowserTestUtils.waitForContentEvent(
          browser,
          "DOMFormRemoved"
        );

        const formSubmissionDetectedPromise =
          BrowserTestUtils.waitForContentEvent(
            browser,
            "form-submission-detected"
          );

        info("Remove address form from DOM");
        await SpecialPowers.spawn(browser, [], async function () {
          let form = content.document.getElementById("form-filled");
          form.remove();
        });

        info("Wait for DOMFormRemoved event");
        await domFormRemovedPromise;

        info("Wait for form-submission-detected event");
        await formSubmissionDetectedPromise;
        // FormAutofill unregisters their interest in form removals after processing the form submission

        // Because FormAutofill has no interest in form removals anymore, the FormHandler should have
        // removed the listeners for the events DOMDocFetchSuccess and DOMFormRemoved. Therefore it should also not
        // dispatch the events before-form-submission and form-submission-detected itself.
        await SpecialPowers.spawn(browser, [], async () => {
          content.document.addEventListener("DOMDocFetchSuccess", () =>
            is(false, "Should not dispatch DOMDocFetchSuccess event")
          );
          content.document.addEventListener("before-form-submission", () =>
            is(false, "Should not dispatch before-form-submission event")
          );
          content.document.addEventListener("DOMFormRemoved", () =>
            is(false, "Should not dispatch DOMFormRemoved event")
          );
          content.document.addEventListener("form-submission-detected", () =>
            is(false, "Should not dispatch form-submission-detected event")
          );
        });

        info(
          "Infer another successfull fetch request and remove another form form the DOM"
        );
        await SpecialPowers.spawn(browser, [], async () => {
          await content.fetch(
            "https://example.org/browser/browser/extensions/formautofill/test/browser/empty.html"
          );
          let form = content.document.getElementById("form-not-filled");
          form.remove();
        });
      }
    );
  }
);
