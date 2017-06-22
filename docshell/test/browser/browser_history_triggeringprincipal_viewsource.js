"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const HTML_URI = TEST_PATH + "dummy_page.html";
const VIEW_SRC_URI = "view-source:" + HTML_URI;

add_task(async function() {
  info("load baseline html in new tab");
  await BrowserTestUtils.withNewTab(HTML_URI, async function(aBrowser) {
    is(gBrowser.selectedBrowser.currentURI.spec, HTML_URI,
       "sanity check to make sure html loaded");

    info("right-click -> view-source of html");
    let vSrcCtxtMenu = document.getElementById("contentAreaContextMenu");
    let popupPromise = BrowserTestUtils.waitForEvent(vSrcCtxtMenu, "popupshown");
    BrowserTestUtils.synthesizeMouseAtCenter("body", { type: "contextmenu", button: 2 }, aBrowser);
    await popupPromise;
    let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, VIEW_SRC_URI);
    let vSrcItem = vSrcCtxtMenu.getElementsByAttribute("id", "context-viewsource")[0];
    vSrcItem.click();
    vSrcCtxtMenu.hidePopup();
    let tab = await tabPromise;
    is(gBrowser.selectedBrowser.currentURI.spec, VIEW_SRC_URI,
       "loading view-source of html succeeded");

    info ("load html file again before going .back()");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, HTML_URI);
    await ContentTask.spawn(tab.linkedBrowser, HTML_URI, HTML_URI => {
      content.document.location = HTML_URI;
    });
    await loadPromise;
    is(gBrowser.selectedBrowser.currentURI.spec, HTML_URI,
      "loading html another time succeeded");

    info("click .back() to view-source of html again and make sure the history entry has a triggeringPrincipal");
    let backCtxtMenu = document.getElementById("contentAreaContextMenu");
    popupPromise = BrowserTestUtils.waitForEvent(backCtxtMenu, "popupshown");
    BrowserTestUtils.synthesizeMouseAtCenter("body", { type: "contextmenu", button: 2 }, aBrowser);
    await popupPromise;
    loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, VIEW_SRC_URI);
    let backItem = backCtxtMenu.getElementsByAttribute("id", "context-back")[0];
    backItem.click();
    backCtxtMenu.hidePopup();
    await loadPromise;
    is(gBrowser.selectedBrowser.currentURI.spec, VIEW_SRC_URI,
      "clicking .back() to view-source of html succeeded");

    await BrowserTestUtils.removeTab(tab);
  });
});
