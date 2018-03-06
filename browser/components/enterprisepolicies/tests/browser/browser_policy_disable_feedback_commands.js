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

function check_all_feedback_commands_hidden(url) {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, async function(browser) {
    // We don't get load events when the DocShell redirects to error
    // pages, but we do get DOMContentLoaded, so we'll wait for that.
    let dclPromise = ContentTask.spawn(browser, null, async function() {
      await ContentTaskUtils.waitForEvent(this, "DOMContentLoaded", false);
    });
    browser.loadURI(url);
    await dclPromise;

    let menu = document.getElementById("menu_HelpPopup");
    let menuOpen = BrowserTestUtils.waitForEvent(menu, "popupshown");
    menu.openPopup(null, "", 0, 0, false, null);
    await menuOpen;

    let feedbackPageMenu = document.getElementById("feedbackPage");
    is(feedbackPageMenu.getAttribute("disabled"), "true",
       "The `Submit Feedback...` item should be disabled");

    let reportMenu =
      document.getElementById("menu_HelpPopup_reportPhishingtoolmenu");
    is(reportMenu.getAttribute("disabled"), "true",
       "The `Report Deceptive Site` item should be disabled");

    let errorMenu =
      document.getElementById("menu_HelpPopup_reportPhishingErrortoolmenu");
    is(errorMenu.getAttribute("disabled"), "true",
       "The `This isn’t a deceptive site` item should be disabled");

    let menuClose = BrowserTestUtils.waitForEvent(menu, "popuphidden");
    menu.hidePopup();
    await menuClose;
  });
}

add_task(async function test_normal_page() {
  return check_all_feedback_commands_hidden(NORMAL_PAGE)
});

add_task(async function test_phishing_page() {
  return check_all_feedback_commands_hidden(PHISH_PAGE)
});
