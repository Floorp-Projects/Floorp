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

add_task(async function test_private_popup_window_opens_private_tabs() {
  // allow top level data: URI navigations, otherwise clicking a data: link fails
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", false]],
  });
  let privWin = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  // Sanity check - this browser better be private.
  ok(
    PrivateBrowsingUtils.isWindowPrivate(privWin),
    "Opened a private browsing window."
  );

  // First, open a private browsing window, and load our
  // testing page.
  let privBrowser = privWin.gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(privBrowser, WINDOW_BODY);
  await BrowserTestUtils.browserLoaded(privBrowser);

  // Next, click on the link in the testing page, and ensure
  // that a private popup window is opened.
  let openedPromise = BrowserTestUtils.waitForNewWindow({ url: POPUP_LINK });

  await BrowserTestUtils.synthesizeMouseAtCenter("#first", {}, privBrowser);
  let popupWin = await openedPromise;
  ok(
    PrivateBrowsingUtils.isWindowPrivate(popupWin),
    "Popup window was private."
  );

  // Now click on the link in the popup, and ensure that a new
  // tab is opened in the original private browsing window.
  let newTabPromise = BrowserTestUtils.waitForNewTab(privWin.gBrowser);
  let popupBrowser = popupWin.gBrowser.selectedBrowser;
  await BrowserTestUtils.synthesizeMouseAtCenter("#second", {}, popupBrowser);
  let newPrivTab = await newTabPromise;

  // Ensure that the newly created tab's browser is private.
  ok(
    PrivateBrowsingUtils.isBrowserPrivate(newPrivTab.linkedBrowser),
    "Newly opened tab should be private."
  );

  // Clean up
  BrowserTestUtils.removeTab(newPrivTab);
  await BrowserTestUtils.closeWindow(popupWin);
  await BrowserTestUtils.closeWindow(privWin);
});
