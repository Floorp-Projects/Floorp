/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 655584 - awesomebar suggestions don't update after tab is closed

add_task(async function() {
  var tab1 = BrowserTestUtils.addTab(gBrowser);
  var tab2 = BrowserTestUtils.addTab(gBrowser);

  // When urlbar in a new tab is focused, and a tab switch occurs,
  // the urlbar popup should be closed
  await BrowserTestUtils.switchTab(gBrowser, tab2);
  gURLBar.focus(); // focus the urlbar in the tab we will switch to
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  gURLBar.openPopup();
  await BrowserTestUtils.switchTab(gBrowser, tab2);
  ok(!gURLBar.popupOpen, "urlbar focused in tab to switch to, close popup");

  // cleanup
  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
