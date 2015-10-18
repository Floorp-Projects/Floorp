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

  gBrowser.addTab();
  gBrowser.addTab();
  gBrowser.addTab();

  checkPreviews(4, "Correct number of previews after adding");

  for (let preview of AeroPeek.previews)
    ok(preview.visible, "Preview is shown as expected");

  gPrefService.setBoolPref(ENABLE_PREF_NAME, false);
  checkPreviews(4, "Previews are unchanged when disabling");

  for (let preview of AeroPeek.previews)
    ok(!preview.visible, "Preview is not shown as expected after disabling");

  gPrefService.setBoolPref(ENABLE_PREF_NAME, true);
  checkPreviews(4, "Previews are unchanged when re-enabling");
  for (let preview of AeroPeek.previews)
    ok(preview.visible, "Preview is shown as expected after re-enabling");

  [1,2,3,4].forEach(function (idx) {
    gBrowser.selectedTab = gBrowser.tabs[idx];
    ok(checkSelectedTab(), "Current tab is correctly selected");
  });

  let currentSelectedTab = gBrowser.selectedTab;
  for (let idx of [1,2,3,4]) {
    let preview = getPreviewForTab(gBrowser.tabs[0]);
    let canvas = createThumbnailSurface(preview);
    let ctx = canvas.getContext("2d");
    preview.controller.drawThumbnail(ctx, canvas.width, canvas.height);
    ok(currentSelectedTab.selected, "Drawing thumbnail does not change selection");
    canvas = getCanvas(preview.controller.width, preview.controller.height);
    ctx = canvas.getContext("2d");
    preview.controller.drawPreview(ctx);
    ok(currentSelectedTab.selected, "Drawing preview does not change selection");
  }

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
  gBrowser.addTab();
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
    gPrefService.clearUserPref(ENABLE_PREF_NAME);

  finish();

  function checkPreviews(aPreviews, msg) {
    let nPreviews = AeroPeek.previews.length;
    is(aPreviews, gBrowser.tabs.length, "Browser has expected number of tabs");
    is(nPreviews, gBrowser.tabs.length, "Browser has one preview per tab");
    is(nPreviews, aPreviews, msg || "Got expected number of previews");
  }

  function getPreviewForTab(tab) {
    return window.gTaskbarTabGroup.previewFromTab(tab);
  }

  function checkSelectedTab() {
    return getPreviewForTab(gBrowser.selectedTab).active;
  }

  function isTabSelected(idx) {
    return gBrowser.tabs[idx].selected;
  }

  function createThumbnailSurface(p) {
    let thumbnailWidth = 200,
        thumbnailHeight = 120;
    let ratio = p.controller.thumbnailAspectRatio;

    if (thumbnailWidth/thumbnailHeight > ratio)
      thumbnailWidth = thumbnailHeight * ratio;
    else
      thumbnailHeight = thumbnailWidth / ratio;

    return getCanvas(thumbnailWidth, thumbnailHeight);
  }

  function getCanvas(width, height) {
    let win = window.QueryInterface(Ci.nsIDOMWindow);
    let doc = win.document;
    let canvas = doc.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.width = width;
    canvas.height = height;
    return canvas;
  }
}
