/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests ensure that the drop preview correctly arranges sites when
 * dragging them around.
 */
add_task(function* () {
  yield* addNewTabPageTab();

  // the first three sites are pinned - make sure they're re-arranged correctly
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1,2,,,5");

  yield* addNewTabPageTab();
  yield* checkGrid("0p,1p,2p,3,4,5p,6,7,8");

  let foundSites = yield ContentTask.spawn(gWindow.gBrowser.selectedBrowser, {}, function*() {
    let cells = content.gGrid.cells;
    content.gDrag._draggedSite = cells[0].site;
    let sites = content.gDropPreview.rearrange(cells[4]);
    content.gDrag._draggedSite = null;

    sites = sites.slice(0, 9);
    return sites.map(function(aSite) {
      if (!aSite)
        return "";

      let pinned = aSite.isPinned();
      if (pinned != aSite.node.hasAttribute("pinned")) {
        Assert.ok(false, "invalid state (site.isPinned() != site[pinned])");
      }

      return aSite.url.replace(/^http:\/\/example(\d+)\.com\/$/, "$1") + (pinned ? "p" : "");
    });
  });

  let expectedSites = "3,1p,2p,4,0p,5p,6,7,8"
  is(foundSites, expectedSites, "grid status = " + expectedSites);
});

