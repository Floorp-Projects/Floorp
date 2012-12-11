function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();

  SpecialPowers.setIntPref("ui.tooltipDelay", 0);

  var popup = false;
  var doc;
  var win;
  var p1;

  let onPopupShown = function(aEvent) {
    popup = true;
  }

  // test that a mouse click prior to the tooltip appearing blocks it
  let runTest = function() {
    EventUtils.synthesizeMouseAtCenter(p1, { type: "mousemove" }, win);
    EventUtils.sendMouseEvent({type:'mousedown'}, p1, win);
    EventUtils.sendMouseEvent({type:'mouseup'}, p1, win);

    setTimeout(function() {
      is(popup, false, "shouldn't get tooltip after click");

      document.removeEventListener("popupshown", onPopupShown, true);
      SpecialPowers.clearUserPref("ui.tooltipDelay");

      gBrowser.removeCurrentTab();
      finish();
    }, 200);
  }

  let onLoad = function (aEvent) {
    doc = gBrowser.contentDocument;
    win = gBrowser.contentWindow;
    p1 = doc.getElementById("p1");

    document.addEventListener("popupshown", onPopupShown, true);

    runTest();
  }

  gBrowser.selectedBrowser.addEventListener("load", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("load", loadListener, true);
    setTimeout(onLoad, 0);
  }, true);

  content.location = "data:text/html," +
    "<p id=\"p1\" title=\"tooltip is here\">This paragraph has a tooltip.</p>";
}
