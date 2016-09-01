/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 655584 - awesomebar suggestions don't update after tab is closed

add_task(function* () {
  var tab1 = gBrowser.addTab();
  var tab2 = gBrowser.addTab();

  // When urlbar in a new tab is focused, and a tab switch occurs,
  // the urlbar popup should be closed
  yield BrowserTestUtils.switchTab(gBrowser, tab2);
  gURLBar.focus(); // focus the urlbar in the tab we will switch to
  yield BrowserTestUtils.switchTab(gBrowser, tab1);
  gURLBar.openPopup();
  yield BrowserTestUtils.switchTab(gBrowser, tab2);
  ok(!gURLBar.popupOpen, "urlbar focused in tab to switch to, close popup");

  // cleanup
  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
