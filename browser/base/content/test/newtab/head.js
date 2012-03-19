/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_NEWTAB_ENABLED = "browser.newtabpage.enabled";

Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, true);

let tmp = {};
Cu.import("resource:///modules/NewTabUtils.jsm", tmp);
let NewTabUtils = tmp.NewTabUtils;

registerCleanupFunction(function () {
  while (gBrowser.tabs.length > 1)
    gBrowser.removeTab(gBrowser.tabs[1]);

  Services.prefs.clearUserPref(PREF_NEWTAB_ENABLED);
});

/**
 * Global variables that are accessed by tests.
 */
let cw;
let cells;

/**
 * We'll want to restore the original links provider later.
 */
let originalProvider = NewTabUtils.links._provider;

/**
 * Provide the default test function to start our test runner.
 */
function test() {
  TestRunner.run();
}

/**
 * The test runner that controls the execution flow of our tests.
 */
let TestRunner = {
  /**
   * Starts the test runner.
   */
  run: function () {
    waitForExplicitFinish();

    this._iter = runTests();
    this.next();
  },

  /**
   * Runs the next available test or finishes if there's no test left.
   */
  next: function () {
    try {
      TestRunner._iter.next();
    } catch (e if e instanceof StopIteration) {
      TestRunner.finish();
    }
  },

  /**
   * Finishes all tests and cleans up.
   */
  finish: function () {
    function cleanupAndFinish() {
      // Restore the old provider.
      NewTabUtils.links._provider = originalProvider;

      whenPagesUpdated(finish);
      NewTabUtils.restore();
    }

    let callbacks = NewTabUtils.links._populateCallbacks;
    let numCallbacks = callbacks.length;

    if (numCallbacks)
      callbacks.splice(0, numCallbacks, cleanupAndFinish);
    else
      cleanupAndFinish();
  }
};

/**
 * Allows to provide a list of links that is used to construct the grid.
 * @param aLinksPattern the pattern (see below)
 *
 * Example: setLinks("1,2,3")
 * Result: [{url: "about:blank#1", title: "site#1"},
 *          {url: "about:blank#2", title: "site#2"}
 *          {url: "about:blank#3", title: "site#3"}]
 */
function setLinks(aLinksPattern) {
  let links = aLinksPattern.split(/\s*,\s*/).map(function (id) {
    return {url: "about:blank#" + id, title: "site#" + id};
  });

  NewTabUtils.links._provider = {getLinks: function (c) c(links)};
  NewTabUtils.links._links = links;
}

/**
 * Allows to specify the list of pinned links (that have a fixed position in
 * the grid.
 * @param aLinksPattern the pattern (see below)
 *
 * Example: setPinnedLinks("3,,1")
 * Result: 'about:blank#3' is pinned in the first cell. 'about:blank#1' is
 *         pinned in the third cell.
 */
function setPinnedLinks(aLinksPattern) {
  let pinnedLinks = [];

  aLinksPattern.split(/\s*,\s*/).forEach(function (id, index) {
    let link;

    if (id)
      link = {url: "about:blank#" + id, title: "site#" + id};

    pinnedLinks[index] = link;
  });

  // Inject the list of pinned links to work with.
  NewTabUtils.pinnedLinks._links = pinnedLinks;
}

/**
 * Restore the grid state.
 */
function restore() {
  whenPagesUpdated();
  NewTabUtils.restore();
}

/**
 * Creates a new tab containing 'about:newtab'.
 */
function addNewTabPageTab() {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:newtab");
  let browser = tab.linkedBrowser;

  // Wait for the new tab page to be loaded.
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    cw = browser.contentWindow;

    if (NewTabUtils.allPages.enabled) {
      // Continue when the link cache has been populated.
      NewTabUtils.links.populateCache(function () {
        cells = cw.gGrid.cells;
        executeSoon(TestRunner.next);
      });
    } else {
      TestRunner.next();
    }

  }, true);
}

/**
 * Compares the current grid arrangement with the given pattern.
 * @param the pattern (see below)
 * @param the array of sites to compare with (optional)
 *
 * Example: checkGrid("3p,2,,1p")
 * Result: We expect the first cell to contain the pinned site 'about:blank#3'.
 *         The second cell contains 'about:blank#2'. The third cell is empty.
 *         The fourth cell contains the pinned site 'about:blank#4'.
 */
function checkGrid(aSitesPattern, aSites) {
  let valid = true;

  aSites = aSites || cw.gGrid.sites;

  aSitesPattern.split(/\s*,\s*/).forEach(function (id, index) {
    let site = aSites[index];
    let match = id.match(/^\d+/);

    // We expect the cell to be empty.
    if (!match) {
      if (site) {
        valid = false;
        ok(false, "expected cell#" + index + " to be empty");
      }

      return;
    }

    // We expect the cell to contain a site.
    if (!site) {
      valid = false;
      ok(false, "didn't expect cell#" + index + " to be empty");

      return;
    }

    let num = match[0];

    // Check the site's url.
    if (site.url != "about:blank#" + num) {
      valid = false;
      is(site.url, "about:blank#" + num, "cell#" + index + " has the wrong url");
    }

    let shouldBePinned = /p$/.test(id);
    let cellContainsPinned = site.isPinned();
    let cssClassPinned = site.node && site.node.querySelector(".newtab-control-pin").hasAttribute("pinned");

    // Check if the site should be and is pinned.
    if (shouldBePinned) {
      if (!cellContainsPinned) {
        valid = false;
        ok(false, "expected cell#" + index + " to be pinned");
      } else if (!cssClassPinned) {
        valid = false;
        ok(false, "expected cell#" + index + " to have css class 'pinned'");
      }
    } else {
      if (cellContainsPinned) {
        valid = false;
        ok(false, "didn't expect cell#" + index + " to be pinned");
      } else if (cssClassPinned) {
        valid = false;
        ok(false, "didn't expect cell#" + index + " to have css class 'pinned'");
      }
    }
  });

  // If every test passed, say so.
  if (valid)
    ok(true, "grid status = " + aSitesPattern);
}

/**
 * Blocks the given cell's site from the grid.
 * @param aCell the cell that contains the site to block
 */
function blockCell(aCell) {
  aCell.site.block(function () executeSoon(TestRunner.next));
}

/**
 * Pins a given cell's site on a given position.
 * @param aCell the cell that contains the site to pin
 * @param aIndex the index the defines where the site should be pinned
 */
function pinCell(aCell, aIndex) {
  aCell.site.pin(aIndex);
}

/**
 * Unpins the given cell's site.
 * @param aCell the cell that contains the site to unpin
 */
function unpinCell(aCell) {
  aCell.site.unpin(function () executeSoon(TestRunner.next));
}

/**
 * Simulates a drop and drop operation.
 * @param aDropTarget the cell that is the drop target
 * @param aDragSource the cell that contains the dragged site (optional)
 */
function simulateDrop(aDropTarget, aDragSource) {
  let event = {
    clientX: 0,
    clientY: 0,
    dataTransfer: {
      mozUserCancelled: false,
      setData: function () null,
      setDragImage: function () null,
      getData: function () "about:blank#99\nblank"
    }
  };

  if (aDragSource)
    cw.gDrag.start(aDragSource.site, event);

  cw.gDrop.drop(aDropTarget, event, function () executeSoon(TestRunner.next));

  if (aDragSource)
    cw.gDrag.end(aDragSource.site);
}

/**
 * Resumes testing when all pages have been updated.
 */
function whenPagesUpdated(aCallback) {
  let page = {
    update: function () {
      NewTabUtils.allPages.unregister(this);
      executeSoon(aCallback || TestRunner.next);
    }
  };

  NewTabUtils.allPages.register(page);
  registerCleanupFunction(function () {
    NewTabUtils.allPages.unregister(page);
  });
}
