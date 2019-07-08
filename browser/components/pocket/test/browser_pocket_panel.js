/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

  info("clicking on pocket button in url bar");
  let pocketButton = document.getElementById("pocket-button");
  pocketButton.click();

  checkElements(true, ["pageActionActivatedActionPanel"]);
  let pocketPanel = document.getElementById("pageActionActivatedActionPanel");
  is(pocketPanel.state, "showing", "pocket panel is showing");

  info("closing pocket panel");
  pocketButton.click();
  checkElements(false, ["pageActionActivatedActionPanel"]);

  BrowserTestUtils.removeTab(tab);
});
