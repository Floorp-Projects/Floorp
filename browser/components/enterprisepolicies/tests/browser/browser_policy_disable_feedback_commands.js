/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  await setupPolicyEngineWithJson({
    "policies": {
      "DisableFeedbackCommands": true
    }
  });
});

const NORMAL_PAGE = "http://example.com";
const PHISH_PAGE = "http://www.itisatrap.org/firefox/its-a-trap.html";

async function prepare_help_menu_items(callback) {
    let menu = document.getElementById("menu_HelpPopup");
    let menuOpen = BrowserTestUtils.waitForEvent(menu, "popupshown");
    menu.openPopup(null, "", 0, 0, false, null);
    await menuOpen;

    callback();

    let menuClose = BrowserTestUtils.waitForEvent(menu, "popuphidden");
    menu.hidePopup();
    await menuClose;
}

add_task(async function test_feedback_item_disabled() {
  return prepare_help_menu_items(() => {
    let feedbackPageMenu = document.getElementById("feedbackPage");
    is(feedbackPageMenu.getAttribute("disabled"), "true",
       "The `Submit Feedback...` item should be disabled");
  });
});

function check_both_report_menu_disabled(url) {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url,
    waitForLoad: false,
    waitForStateStop: true
  }, async function(browser) {
    await prepare_help_menu_items(() => {
      let reportMenu =
        document.getElementById("menu_HelpPopup_reportPhishingtoolmenu");
      is(reportMenu.getAttribute("disabled"), "true",
         "The `Report Deceptive Site` item should be disabled");

      let errorMenu =
        document.getElementById("menu_HelpPopup_reportPhishingErrortoolmenu");
      is(errorMenu.getAttribute("disabled"), "true",
         "The `This isn’t a deceptive site` item should be disabled");
    });
  });
}

add_task(async function test_report_menu_normal_page() {
  return check_both_report_menu_disabled(NORMAL_PAGE);
});

add_task(async function test_report_menu_phishing_page() {
  return check_both_report_menu_disabled(PHISH_PAGE);
});
