/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PRELOAD_PREF = "browser.newtab.preload";

function runTests() {
  // turn off preload to ensure that a newtab page loads
  Services.prefs.setBoolPref(PRELOAD_PREF, false);

  // add a test provider that waits for load
  let afterLoadProvider = {
    getLinks: function(callback) {
      this.callback = callback;
    },
    addObserver: function() {},
  };
  NewTabUtils.links.addProvider(afterLoadProvider);

  // wait until about:newtab loads before calling provider callback
  addNewTabPageTab();
  let browser = gWindow.gBrowser.selectedBrowser;
  yield browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    // afterLoadProvider.callback has to be called asynchronously to make grid
    // initilize after "load" event was handled
    executeSoon(() => afterLoadProvider.callback([]));
  }, true);

  let {_cellMargin, _cellHeight, _cellWidth, node} = getGrid();
  isnot(_cellMargin, null, "grid has a computed cell margin");
  isnot(_cellHeight, null, "grid has a computed cell height");
  isnot(_cellWidth, null, "grid has a computed cell width");
  let {height, maxHeight, maxWidth} = node.style;
  isnot(height, "", "grid has a computed grid height");
  isnot(maxHeight, "", "grid has a computed grid max-height");
  isnot(maxWidth, "", "grid has a computed grid max-width");

  // restore original state
  NewTabUtils.links.removeProvider(afterLoadProvider);
  Services.prefs.clearUserPref(PRELOAD_PREF);
}
