"use strict";

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

const FIRST_TAB = getRootDirectory(gTestPath) + "close_beforeunload_opens_second_tab.html";
const SECOND_TAB = getRootDirectory(gTestPath) + "close_beforeunload.html";

add_task(function*() {
  let firstTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, FIRST_TAB);
  let newTabPromise = waitForNewTabEvent(gBrowser);
  ContentTask.spawn(firstTab.linkedBrowser, "", function*() {
    content.document.getElementsByTagName("a")[0].click();
  });
  let tabOpenEvent = yield newTabPromise;
  let secondTab = tabOpenEvent.target;
  yield ContentTask.spawn(secondTab.linkedBrowser, SECOND_TAB, function*(expectedURL) {
    if (content.window.location.href == expectedURL &&
        content.document.readyState === "complete") {
      return Promise.resolve();
    }
    return new Promise(function(resolve, reject) {
      content.window.addEventListener("load", function() {
        if (content.window.location.href == expectedURL) {
          resolve();
        }
      }, false);
    });
  });

  let closeBtn = document.getAnonymousElementByAttribute(secondTab, "anonid", "close-button");
  let closePromise = BrowserTestUtils.removeTab(secondTab, {dontRemove: true});
  info("closing second tab (which will self-close in beforeunload)");
  closeBtn.click();
  ok(secondTab.closing, "Second tab should be marked as closing synchronously.");
  yield closePromise;
  ok(secondTab.closing, "Second tab should still be marked as closing");
  ok(!secondTab.linkedBrowser, "Second tab's browser should be dead");
  ok(!firstTab.closing, "First tab should not be closing");
  ok(firstTab.linkedBrowser, "First tab's browser should be alive");
  info("closing first tab");
  yield BrowserTestUtils.removeTab(firstTab);

  ok(firstTab.closing, "First tab should be marked as closing");
  ok(!firstTab.linkedBrowser, "First tab's browser should be dead");
});
