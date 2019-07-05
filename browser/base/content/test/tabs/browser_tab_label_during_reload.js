/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:preferences"
  ));
  let browser = tab.linkedBrowser;
  let labelChanges = 0;
  let attrModifiedListener = event => {
    if (event.detail.changed.includes("label")) {
      labelChanges++;
    }
  };
  tab.addEventListener("TabAttrModified", attrModifiedListener);

  await BrowserTestUtils.browserLoaded(browser);
  is(labelChanges, 1, "number of label changes during initial load");
  isnot(tab.label, "", "about:preferences tab label isn't empty");
  isnot(
    tab.label,
    "about:preferences",
    "about:preferences tab label isn't the URI"
  );
  is(
    tab.label,
    browser.contentTitle,
    "about:preferences tab label matches browser.contentTitle"
  );

  labelChanges = 0;
  browser.reload();
  await BrowserTestUtils.browserLoaded(browser);
  is(labelChanges, 0, "number of label changes during reload");

  tab.removeEventListener("TabAttrModified", attrModifiedListener);
  gBrowser.removeTab(tab);
});
