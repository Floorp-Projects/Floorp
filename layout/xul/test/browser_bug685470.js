add_task(function* () {
  const html = "<p id=\"p1\" title=\"tooltip is here\">This paragraph has a tooltip.</p>";
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/html," + html);

  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [["ui.tooltipDelay", 0]]}, resolve);
  });

  var popup = false;
  var doc;
  var win;
  var p1;

  let onPopupShown = function(aEvent) {
    popup = true;
  }
  document.addEventListener("popupshown", onPopupShown, true);

  // Send a mousemove at a known position to start the test.
  BrowserTestUtils.synthesizeMouseAtCenter("#p1", { type: "mousemove" },
                                           gBrowser.selectedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#p1", { }, gBrowser.selectedBrowser);

  yield new Promise(resolve => {
    setTimeout(function() {
      is(popup, false, "shouldn't get tooltip after click");
      resolve();
    }, 200);
  });

  document.removeEventListener("popupshown", onPopupShown, true);
  gBrowser.removeCurrentTab();
});
