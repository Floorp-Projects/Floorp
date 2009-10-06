function test() {
  waitForExplicitFinish();

  // Cannot do anything if the taskbar service is not available
  ok(AeroPeek.available == isWin7OrHigher(), "AeroPeek initialized when it should be");

  // Entire feature is disabled
  if (!AeroPeek.available)
    return;

  gPrefService.setBoolPref("aeropeek.enable", true);

  is(1, AeroPeek.windows.length, "Got the expected number of windows");

  checkPreviews(1, "Browser starts with one preview");

  gBrowser.addTab();
  gBrowser.addTab();
  gBrowser.addTab();

  checkPreviews(4, "Correct number of previews after adding");

  for each (let preview in AeroPeek.previews)
    ok(preview.visible, "Preview is shown as expected");

  gPrefService.setBoolPref("aeropeek.enable", false);
  checkPreviews(4, "Previews are unchanged when disabling");

  for each (let preview in AeroPeek.previews)
    ok(!preview.visible, "Preview is not shown as expected after disabling");

  gPrefService.setBoolPref("aeropeek.enable", true);
  checkPreviews(4, "Previews are unchanged when re-enabling");
  for each (let preview in AeroPeek.previews)
    ok(preview.visible, "Preview is shown as expected after re-enabling");

  [1,2,3,4].forEach(function (idx) {
    gBrowser.selectedTab = gBrowser.mTabs[idx];
    ok(checkSelectedTab(), "Current tab is correctly selected");
  });

  let currentSelectedTab = gBrowser.selectedTab;
  for each (let idx in [1,2,3,4]) {
    let preview = getPreviewForTab(gBrowser.mTabs[0]);
    let canvas = createThumbnailSurface(preview);
    let ctx = canvas.getContext("2d");
    preview.controller.drawThumbnail(ctx, canvas.width, canvas.height);
    ok(currentSelectedTab == gBrowser.selectedTab, "Drawing thumbnail does not change selection");
    canvas = getCanvas(preview.controller.width, preview.controller.height);
    ctx = canvas.getContext("2d");
    preview.controller.drawPreview(ctx);
    ok(currentSelectedTab == gBrowser.selectedTab, "Drawing preview does not change selection");
  }

  // Close #4
  getPreviewForTab(gBrowser.selectedTab).controller.onClose();
  checkPreviews(3, "Expected number of previews after closing selected tab via controller");
  ok(gBrowser.mTabs.length == 3, "Successfully closed a tab");

  // Select #1
  ok(getPreviewForTab(gBrowser.mTabs[0]).controller.onActivate(), "Activation was accepted");
  ok(gBrowser.mTabs[0] == gBrowser.selectedTab, "Correct tab was selected");
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
  gBrowser.selectedTab = gBrowser.mTabs[0];
  checkSelectedTab();
  // Close nonselected tab via controller
  getPreviewForTab(gBrowser.mTabs[1]).controller.onClose();
  checkPreviews(1);

  gPrefService.clearUserPref("aeropeek.enable");

  finish();

  function checkPreviews(aPreviews, msg) {
    let nPreviews = AeroPeek.previews.length;
    is(aPreviews, gBrowser.mTabs.length, "Browser has expected number of tabs");
    is(nPreviews, gBrowser.mTabs.length, "Browser has one preview per tab");
    is(nPreviews, aPreviews, msg || "Got expected number of previews");
  }

  function isWin7OrHigher() {
    try {
      var sysInfo = Cc["@mozilla.org/system-info;1"].
                    getService(Ci.nsIPropertyBag2);
      var ver = parseFloat(sysInfo.getProperty("version"));
      if (ver >= 6.1)
        return true;
    } catch (ex) { }
    return false;
  }
  function getPreviewForTab(tab)
    window.gTaskbarTabGroup.previewFromTab(tab);

  function checkSelectedTab()
    getPreviewForTab(gBrowser.selectedTab).active;

  function isTabSelected(idx)
    gBrowser.selectedTab == gBrowser.mTabs[idx];

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
    let win = window.QueryInterface(Ci.nsIDOMWindowInternal);
    let doc = win.document;
    let canvas = doc.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.width = width;
    canvas.height = height;
    return canvas;
  }
}
