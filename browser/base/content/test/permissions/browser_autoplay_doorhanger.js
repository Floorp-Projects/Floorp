/*
 * Test that autoplay doorhanger is hidden when user clicks back to new tab page
 */

const AUTOPLAY_PAGE  = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "browser_autoplay_blocked.html";

add_task(async function testHiddenAfterBack() {

  Services.prefs.setIntPref("media.autoplay.default", Ci.nsIAutoplay.PROMPT);

  await BrowserTestUtils.withNewTab("about:home", async function(browser) {

    is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");

    let shown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    let loaded = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.loadURI(browser, AUTOPLAY_PAGE);
    await loaded;
    await shown;

    is(PopupNotifications.panel.state, "open", "Doorhanger is shown");

    let hidden = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
    let backPromise = BrowserTestUtils.browserStopped(browser);
    EventUtils.synthesizeMouseAtCenter(document.getElementById("back-button"), {});
    await backPromise;
    await hidden;

    is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
  });

  Services.prefs.clearUserPref("media.autoplay.default");
});
