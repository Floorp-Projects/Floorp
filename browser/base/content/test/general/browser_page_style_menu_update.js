"use strict";

const PAGE = "http://example.com/browser/browser/base/content/test/general/page_style_sample.html";

/**
 * Stylesheets are updated for a browser after the pageshow event.
 * This helper function returns a Promise that waits for that pageshow
 * event, and then resolves on the next tick to ensure that gPageStyleMenu
 * has had a chance to update the stylesheets.
 *
 * @param browser
 *        The <xul:browser> to wait for.
 * @return Promise
 */
function promiseStylesheetsUpdated(browser) {
  return ContentTask.spawn(browser, { PAGE }, function*(args) {
    return new Promise((resolve) => {
      addEventListener("pageshow", function onPageShow(e) {
        if (e.target.location == args.PAGE) {
          removeEventListener("pageshow", onPageShow);
          content.setTimeout(resolve, 0);
        }
      });
    })
  });
}

/**
 * Tests that the Page Style menu shows the currently
 * selected Page Style after a new one has been selected.
 */
add_task(function*() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", false);
  let browser = tab.linkedBrowser;

  yield BrowserTestUtils.loadURI(browser, PAGE);
  yield promiseStylesheetsUpdated(browser);

  let menupopup = document.getElementById("pageStyleMenu").menupopup;
  gPageStyleMenu.fillPopup(menupopup);

  // page_style_sample.html should default us to selecting the stylesheet
  // with the title "6" first.
  let selected = menupopup.querySelector("menuitem[checked='true']");
  is(selected.getAttribute("label"), "6", "Should have '6' stylesheet selected by default");

  // Now select stylesheet "1"
  let targets = menupopup.querySelectorAll("menuitem");
  let target = menupopup.querySelector("menuitem[label='1']");
  target.click();

  // Now we need to wait for the content process to send its stylesheet
  // update for the selected tab to the parent. Because messages are
  // guaranteed to be sent in order, we'll make sure we do the check
  // after the parent has been updated by yielding until the child
  // has finished running a ContentTask for us.
  yield ContentTask.spawn(browser, {}, function*() {
    dump('\nJust wasting some time.\n');
  });

  gPageStyleMenu.fillPopup(menupopup);
  // gPageStyleMenu empties out the menu between opens, so we need
  // to get a new reference to the selected menuitem
  selected = menupopup.querySelector("menuitem[checked='true']");
  is(selected.getAttribute("label"), "1", "Should now have stylesheet 1 selected");

  yield BrowserTestUtils.removeTab(tab);
});
