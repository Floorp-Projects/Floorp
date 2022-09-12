/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

async function promiseValidityPopupShown() {
  await BrowserTestUtils.waitForEvent(
    window,
    "popupshown",
    /* capture = */ false,
    event => event.target.id == "invalid-form-popup"
  );
  return document.getElementById("invalid-form-popup");
}

const HTML = `
  <form action="">
    <input name="text" type="text" placeholder="type here" autofocus required>
    <input id="submit" type="submit">
  </form>
`;

add_task(async function bug_1790128() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `data:text/html,${encodeURI(HTML)}`,
    },
    async function(aBrowser) {
      let promisePopupShown = promiseValidityPopupShown();
      await BrowserTestUtils.synthesizeMouseAtCenter("#submit", {}, aBrowser);
      let popup = await promisePopupShown;
      is(popup.state, "open", "Should be open");

      let promisePopupHidden = BrowserTestUtils.waitForEvent(
        popup,
        "popuphidden"
      );
      promisePopupShown = promiseValidityPopupShown();

      // This is written in a rather odd style because depending on whether
      // panel animations are enabled we might be able to show the popup again
      // after the first click or not. The point is that after one click the
      // popup should've hid at least once, and after two the popup should've
      // open again at least once.
      await BrowserTestUtils.synthesizeMouseAtCenter("#submit", {}, aBrowser);
      await promisePopupHidden;
      await BrowserTestUtils.synthesizeMouseAtCenter("#submit", {}, aBrowser);
      await promisePopupShown;
      ok(true, "Should've shown the popup again");
    }
  );
});
