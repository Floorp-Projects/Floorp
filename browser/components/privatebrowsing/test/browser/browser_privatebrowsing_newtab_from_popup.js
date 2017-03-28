/**
 * Tests that a popup window in private browsing window opens
 * new tab links in the original private browsing window as
 * new tabs.
 *
 * This is a regression test for bug 1202634.
 */

// We're able to sidestep some quote-escaping issues when
// nesting data URI's by encoding the second data URI in
// base64.
const POPUP_BODY_BASE64 = btoa(`<a href="http://example.com/" target="_blank"
                                   id="second">
                                  Now click this
                                </a>`);
const POPUP_LINK = `data:text/html;charset=utf-8;base64,${POPUP_BODY_BASE64}`;
const WINDOW_BODY = `data:text/html,
                     <a href="%23" id="first"
                        onclick="window.open('${POPUP_LINK}', '_blank',
                                             'width=630,height=500')">
                       First click this.
                     </a>`;

add_task(function* test_private_popup_window_opens_private_tabs() {
  let privWin = yield BrowserTestUtils.openNewBrowserWindow({ private: true });

  // Sanity check - this browser better be private.
  ok(PrivateBrowsingUtils.isWindowPrivate(privWin),
     "Could not open a private browsing window.");

  // First, open a private browsing window, and load our
  // testing page.
  let privBrowser = privWin.gBrowser.selectedBrowser;
  yield BrowserTestUtils.loadURI(privBrowser, WINDOW_BODY);
  yield BrowserTestUtils.browserLoaded(privBrowser);

  // Next, click on the link in the testing page, and ensure
  // that a private popup window is opened.
  let openedPromise = BrowserTestUtils.waitForNewWindow();
  yield BrowserTestUtils.synthesizeMouseAtCenter("#first", {}, privBrowser);
  let popupWin = yield openedPromise;
  ok(PrivateBrowsingUtils.isWindowPrivate(popupWin),
     "Popup window was not private.");

  // Now click on the link in the popup, and ensure that a new
  // tab is opened in the original private browsing window.
  let newTabPromise = BrowserTestUtils.waitForNewTab(privWin.gBrowser);
  let popupBrowser = popupWin.gBrowser.selectedBrowser;
  yield BrowserTestUtils.browserLoaded(popupBrowser);
  yield BrowserTestUtils.synthesizeMouseAtCenter("#second", {}, popupBrowser);
  let newPrivTab = yield newTabPromise;

  // Ensure that the newly created tab's browser is private.
  ok(PrivateBrowsingUtils.isBrowserPrivate(newPrivTab.linkedBrowser),
     "Newly opened tab should be private.");

  // Clean up
  yield BrowserTestUtils.removeTab(newPrivTab);
  yield BrowserTestUtils.closeWindow(popupWin);
  yield BrowserTestUtils.closeWindow(privWin);
});
