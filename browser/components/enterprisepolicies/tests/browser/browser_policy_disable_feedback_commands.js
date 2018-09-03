/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* the buidHelpMenu() function comes from browser/base/content/utilityOverlay.js */

const NORMAL_PAGE = "http://example.com";
const PHISH_PAGE = "http://www.itisatrap.org/firefox/its-a-trap.html";

async function checkItemsAreDisabled(url) {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url,
    // The phishing page doesn't send a load notification
    waitForLoad: false,
    waitForStateStop: true,
  }, async function checkItems() {
    buildHelpMenu();

    let reportMenu = document.getElementById("menu_HelpPopup_reportPhishingtoolmenu");
    is(reportMenu.getAttribute("disabled"), "true",
       "The `Report Deceptive Site` item should be disabled");

    let errorMenu = document.getElementById("menu_HelpPopup_reportPhishingErrortoolmenu");
    is(errorMenu.getAttribute("disabled"), "true",
       "The `This isn’t a deceptive site` item should be disabled");
  });
}

add_task(async function test_policy_feedback_commands() {
  await setupPolicyEngineWithJson({
    "policies": {
      "DisableFeedbackCommands": true,
    },
  });

  /* from browser/base/content/utilityOverlay.js */
  buildHelpMenu();

  let feedbackPageMenu = document.getElementById("feedbackPage");
  is(feedbackPageMenu.getAttribute("disabled"), "true",
     "The `Submit Feedback...` item should be disabled");

  await checkItemsAreDisabled(NORMAL_PAGE);
  await checkItemsAreDisabled(PHISH_PAGE);

});

