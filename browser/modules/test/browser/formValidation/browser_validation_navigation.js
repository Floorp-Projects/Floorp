/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure that the form validation message disappears if we navigate
 * immediately.
 */
add_task(async function test_navigate() {
  var gInvalidFormPopup =
    gBrowser.selectedBrowser.browsingContext.currentWindowGlobal
      .getActor("FormValidation")
      ._getAndMaybeCreatePanel(document);
  ok(
    gInvalidFormPopup,
    "The browser should have a popup to show when a form is invalid"
  );

  await BrowserTestUtils.withNewTab(
    "data:text/html,<body contenteditable='true'><button>Click me",
    async function checkTab(browser) {
      let promiseExampleLoaded = BrowserTestUtils.waitForNewTab(
        browser.getTabBrowser(),
        "https://example.com/",
        true
      );
      await SpecialPowers.spawn(browser, [], () => {
        let doc = content.document;
        let input = doc.createElement("select");
        input.style.opacity = 0;
        doc.body.append(input);
        input.setCustomValidity("This message should not show up.");
        content.eval(
          `document.querySelector("button").setAttribute("onmousedown", "document.querySelector('select').reportValidity();window.open('https://example.com/');")`
        );
      });
      await BrowserTestUtils.synthesizeMouseAtCenter("button", {}, browser);
      let otherTab = await promiseExampleLoaded;
      await BrowserTestUtils.waitForPopupEvent(gInvalidFormPopup, "hidden");
      is(
        gInvalidFormPopup.state,
        "closed",
        "Invalid form popup should go away."
      );
      BrowserTestUtils.removeTab(otherTab);
    }
  );
});
