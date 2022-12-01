/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function waitForBlockedPopups(numberOfPopups, { doc }) {
  let toolbarDoc = doc || document;
  let menupopup = toolbarDoc.getElementById("blockedPopupOptions");
  await BrowserTestUtils.waitForCondition(() => {
    let popups = menupopup.querySelectorAll("[popupReportIndex]");
    return popups.length == numberOfPopups;
  }, `Waiting for ${numberOfPopups} popups`);
}
