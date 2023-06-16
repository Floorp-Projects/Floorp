/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gInvalidFormPopup =
  gBrowser.selectedBrowser.browsingContext.currentWindowGlobal
    .getActor("FormValidation")
    ._getAndMaybeCreatePanel(document);

add_task(async function test_other_popup_closes() {
  ok(
    gInvalidFormPopup,
    "The browser should have a popup to show when a form is invalid"
  );
  await BrowserTestUtils.withNewTab(
    "https://example.com/nothere",
    async function checkTab(browser) {
      let popupShown = BrowserTestUtils.waitForPopupEvent(
        gInvalidFormPopup,
        "shown"
      );
      await SpecialPowers.spawn(browser, [], () => {
        let doc = content.document;
        let input = doc.createElement("input");
        doc.body.append(input);
        input.setCustomValidity("This message should be hidden.");
        content.eval(`document.querySelector('input').reportValidity();`);
      });
      let popupHidden = BrowserTestUtils.waitForPopupEvent(
        gInvalidFormPopup,
        "hidden"
      );
      await popupShown;
      let notificationPopup = document.getElementById("notification-popup");
      let notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      let notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );
      await SpecialPowers.spawn(browser, [], () => {
        content.navigator.geolocation.getCurrentPosition(function () {});
      });
      await notificationShown;
      // Should already be hidden at this point.
      is(
        gInvalidFormPopup.state,
        "closed",
        "Form validation popup should have closed"
      );
      // Close just in case.
      if (gInvalidFormPopup.state != "closed") {
        gInvalidFormPopup.hidePopup();
      }
      await popupHidden;
      notificationPopup.hidePopup();
      await notificationHidden;
    }
  );
});

add_task(async function test_dont_open_while_other_popup_open() {
  ok(
    gInvalidFormPopup,
    "The browser should have a popup to show when a form is invalid"
  );
  await BrowserTestUtils.withNewTab(
    "https://example.org/nothere",
    async function checkTab(browser) {
      let notificationPopup = document.getElementById("notification-popup");
      let notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      await SpecialPowers.spawn(browser, [], () => {
        content.navigator.geolocation.getCurrentPosition(function () {});
      });
      await notificationShown;
      let popupShown = BrowserTestUtils.waitForPopupEvent(
        gInvalidFormPopup,
        "shown"
      );
      is(
        gInvalidFormPopup.state,
        "closed",
        "Form validation popup should be closed."
      );
      await SpecialPowers.spawn(browser, [], () => {
        let doc = content.document;
        let input = doc.createElement("input");
        doc.body.append(input);
        input.setCustomValidity("This message should be hidden.");
        content.eval(`document.querySelector('input').reportValidity();`);
      });
      is(
        gInvalidFormPopup.state,
        "closed",
        "Form validation popup should still be closed."
      );
      let notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );
      notificationPopup
        .querySelector(".popup-notification-secondary-button")
        .click();
      await notificationHidden;
      await SpecialPowers.spawn(browser, [], () => {
        content.eval(`document.querySelector('input').reportValidity();`);
      });
      await popupShown;
      let popupHidden = BrowserTestUtils.waitForPopupEvent(
        gInvalidFormPopup,
        "hidden"
      );
      gInvalidFormPopup.hidePopup();
      await popupHidden;
    }
  );
});
