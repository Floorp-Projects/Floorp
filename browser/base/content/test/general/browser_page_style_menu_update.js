"use strict";

const PAGE =
  "http://example.com/browser/browser/base/content/test/general/page_style_sample.html";

/**
 * Tests that the Page Style menu shows the currently
 * selected Page Style after a new one has been selected.
 */
add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    false
  );
  let browser = tab.linkedBrowser;

  await BrowserTestUtils.loadURI(browser, PAGE);
  await promiseStylesheetsUpdated(browser);

  let menupopup = document.getElementById("pageStyleMenu").menupopup;
  gPageStyleMenu.fillPopup(menupopup);

  // page_style_sample.html should default us to selecting the stylesheet
  // with the title "6" first.
  let selected = menupopup.querySelector("menuitem[checked='true']");
  is(
    selected.getAttribute("label"),
    "6",
    "Should have '6' stylesheet selected by default"
  );

  // Now select stylesheet "1"
  let target = menupopup.querySelector("menuitem[label='1']");
  target.click();

  // Now we need to wait for the content process to send its stylesheet
  // update for the selected tab to the parent.
  await promiseStylesheetsUpdated(browser);

  gPageStyleMenu.fillPopup(menupopup);
  // gPageStyleMenu empties out the menu between opens, so we need
  // to get a new reference to the selected menuitem
  selected = menupopup.querySelector("menuitem[checked='true']");
  is(
    selected.getAttribute("label"),
    "1",
    "Should now have stylesheet 1 selected"
  );

  BrowserTestUtils.removeTab(tab);
});
