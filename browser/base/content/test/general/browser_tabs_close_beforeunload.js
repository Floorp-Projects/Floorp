"use strict";

SimpleTest.requestCompleteLog();

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

const FIRST_TAB = getRootDirectory(gTestPath) + "close_beforeunload_opens_second_tab.html";
const SECOND_TAB = getRootDirectory(gTestPath) + "close_beforeunload.html";

add_task(function*() {
  info("Opening first tab");
  let firstTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, FIRST_TAB);
  let secondTabLoadedPromise;
  let secondTab;
  let tabOpened = new Promise(resolve => {
    info("Adding tabopen listener");
    gBrowser.tabContainer.addEventListener("TabOpen", function tabOpenListener(e) {
      info("Got tabopen, removing listener and waiting for load");
      gBrowser.tabContainer.removeEventListener("TabOpen", tabOpenListener, false, false);
      secondTab = e.target;
      secondTabLoadedPromise = BrowserTestUtils.browserLoaded(secondTab.linkedBrowser, false, SECOND_TAB);
      resolve();
    }, false, false);
  });
  info("Opening second tab using a click");
  yield ContentTask.spawn(firstTab.linkedBrowser, "", function*() {
    content.document.getElementsByTagName("a")[0].click();
  });
  info("Waiting for the second tab to be opened");
  yield tabOpened;
  info("Waiting for the load in that tab to finish");
  yield secondTabLoadedPromise;

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
