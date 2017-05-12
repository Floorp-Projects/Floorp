/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  // turn off preload to ensure that a newtab page loads
  await pushPrefs(["browser.newtab.preload", false]);

  // add a test provider that waits for load
  let afterLoadProvider = {
    getLinks(callback) {
      this.callback = callback;
    },
    addObserver() {},
  };
  NewTabUtils.links.addProvider(afterLoadProvider);

  // wait until about:newtab loads before calling provider callback
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:newtab");

  afterLoadProvider.callback([]);

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    let {_cellHeight, _cellWidth, node} = content.gGrid;
    Assert.notEqual(_cellHeight, null, "grid has a computed cell height");
    Assert.notEqual(_cellWidth, null, "grid has a computed cell width");
    let {height, maxHeight, maxWidth} = node.style;
    Assert.notEqual(height, "", "grid has a computed grid height");
    Assert.notEqual(maxHeight, "", "grid has a computed grid max-height");
    Assert.notEqual(maxWidth, "", "grid has a computed grid max-width");
  });

  // restore original state
  NewTabUtils.links.removeProvider(afterLoadProvider);
});
