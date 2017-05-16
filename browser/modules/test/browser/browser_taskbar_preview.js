function test() {
  var isWin7OrHigher = false;
  try {
    let version = Cc["@mozilla.org/system-info;1"]
                    .getService(Ci.nsIPropertyBag2)
                    .getProperty("version");
    isWin7OrHigher = (parseFloat(version) >= 6.1);
  } catch (ex) { }

  is(!!Win7Features, isWin7OrHigher, "Win7Features available when it should be");
  if (!isWin7OrHigher)
    return;

  const ENABLE_PREF_NAME = "browser.taskbar.previews.enable";

  let temp = {};
  Cu.import("resource:///modules/WindowsPreviewPerTab.jsm", temp);
  let AeroPeek = temp.AeroPeek;

  waitForExplicitFinish();

  gPrefService.setBoolPref(ENABLE_PREF_NAME, true);

  is(1, AeroPeek.windows.length, "Got the expected number of windows");

  checkPreviews(1, "Browser starts with one preview");

  BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.addTab(gBrowser);

  checkPreviews(4, "Correct number of previews after adding");

  for (let preview of AeroPeek.previews)
    ok(preview.visible, "Preview is shown as expected");

  gPrefService.setBoolPref(ENABLE_PREF_NAME, false);
  is(0, AeroPeek.previews.length, "Should have 0 previews when disabled");

  gPrefService.setBoolPref(ENABLE_PREF_NAME, true);
  checkPreviews(4, "Previews are back when re-enabling");
  for (let preview of AeroPeek.previews)
    ok(preview.visible, "Preview is shown as expected after re-enabling");

  [1, 2, 3, 4].forEach(function(idx) {
    gBrowser.selectedTab = gBrowser.tabs[idx];
    ok(checkSelectedTab(), "Current tab is correctly selected");
  });

  // Close #4
  getPreviewForTab(gBrowser.selectedTab).controller.onClose();
  checkPreviews(3, "Expected number of previews after closing selected tab via controller");
  ok(gBrowser.tabs.length == 3, "Successfully closed a tab");

  // Select #1
  ok(getPreviewForTab(gBrowser.tabs[0]).controller.onActivate(), "Activation was accepted");
  ok(gBrowser.tabs[0].selected, "Correct tab was selected");
  checkSelectedTab();

  // Remove #3 (non active)
  gBrowser.removeTab(gBrowser.tabContainer.lastChild);
  checkPreviews(2, "Expected number of previews after closing unselected via browser");

  // Remove #1 (active)
  gBrowser.removeTab(gBrowser.tabContainer.firstChild);
  checkPreviews(1, "Expected number of previews after closing selected tab via browser");

  // Add a new tab
  BrowserTestUtils.addTab(gBrowser);
  checkPreviews(2);
  // Check default selection
  checkSelectedTab();

  // Change selection
  gBrowser.selectedTab = gBrowser.tabs[0];
  checkSelectedTab();
  // Close nonselected tab via controller
  getPreviewForTab(gBrowser.tabs[1]).controller.onClose();
  checkPreviews(1);

  if (gPrefService.prefHasUserValue(ENABLE_PREF_NAME))
    gPrefService.setBoolPref(ENABLE_PREF_NAME, !gPrefService.getBoolPref(ENABLE_PREF_NAME));

  finish();

  function checkPreviews(aPreviews, msg) {
    let nPreviews = AeroPeek.previews.length;
    is(aPreviews, gBrowser.tabs.length, "Browser has expected number of tabs - " + msg);
    is(nPreviews, gBrowser.tabs.length, "Browser has one preview per tab - " + msg);
    is(nPreviews, aPreviews, msg || "Got expected number of previews");
  }

  function getPreviewForTab(tab) {
    return window.gTaskbarTabGroup.previewFromTab(tab);
  }

  function checkSelectedTab() {
    return getPreviewForTab(gBrowser.selectedTab).active;
  }
}
