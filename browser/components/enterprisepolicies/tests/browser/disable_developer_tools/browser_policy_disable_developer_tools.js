/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
var updateService = Cc["@mozilla.org/updates/update-service;1"].
                    getService(Ci.nsIApplicationUpdateService);

add_task(async function test_updates_post_policy() {
  is(Services.policies.isAllowed("devtools"), false,
     "devtools should be disabled by policy.");

  is(Services.prefs.getBoolPref("devtools.policy.disabled"), true,
     "devtools dedicated disabled pref is set to true");

  Services.prefs.setBoolPref("devtools.policy.disabled", false);

  is(Services.prefs.getBoolPref("devtools.policy.disabled"), true,
     "devtools dedicated disabled pref can not be updated");

  await expectErrorPage("about:devtools");
  await expectErrorPage("about:devtools-toolbox");
  await expectErrorPage("about:debugging");

  let testURL = "data:text/html;charset=utf-8,test";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, testURL, false);

  info("Check that devtools menu items are hidden");
  let toolsMenu = window.document.getElementById("webDeveloperMenu");
  ok(toolsMenu.hidden, "The Web Developer item of the tools menu is hidden");
  let hamburgerMenu = window.document.getElementById("appMenu-developer-button");
  ok(hamburgerMenu.hidden, "The Web Developer item of the hamburger menu is hidden");

  BrowserTestUtils.removeTab(tab);
});

const expectErrorPage = async function(url) {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, async (browser) => {
    BrowserTestUtils.loadURI(browser, url);
    await BrowserTestUtils.browserLoaded(browser, false, url, true);
    await ContentTask.spawn(browser, url, async function() {
      ok(content.document.documentURI.startsWith("about:neterror?e=blockedByPolicy"),
      content.document.documentURI + " should start with about:neterror?e=blockedByPolicy");
    });
  });
};
