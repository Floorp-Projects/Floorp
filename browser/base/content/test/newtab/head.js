/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_NEWTAB_ENABLED = "browser.newtabpage.enabled";

Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, true);

let tmp = {};
Cu.import("resource:///modules/NewTabUtils.jsm", tmp);
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://browser/content/sanitize.js", tmp);

let {NewTabUtils, Sanitizer} = tmp;

let uri = Services.io.newURI("about:newtab", null, null);
let principal = Services.scriptSecurityManager.getCodebasePrincipal(uri);

let sm = Services.domStorageManager;
let storage = sm.getLocalStorageForPrincipal(principal, "");

registerCleanupFunction(function () {
  while (gBrowser.tabs.length > 1)
    gBrowser.removeTab(gBrowser.tabs[1]);

  Services.prefs.clearUserPref(PREF_NEWTAB_ENABLED);
});

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
      clearHistory();
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
 * Returns the selected tab's content window.
 * @return The content window.
 */
function getContentWindow() {
  return gBrowser.selectedBrowser.contentWindow;
}

/**
 * Returns the selected tab's content document.
 * @return The content document.
 */
function getContentDocument() {
  return gBrowser.selectedBrowser.contentDocument;
}

/**
 * Returns the newtab grid of the selected tab.
 * @return The newtab grid.
 */
function getGrid() {
  return getContentWindow().gGrid;
}

/**
 * Returns the cell at the given index of the selected tab's newtab grid.
 * @param aIndex The cell index.
 * @return The newtab cell.
 */
function getCell(aIndex) {
  return getGrid().cells[aIndex];
}

/**
 * Allows to provide a list of links that is used to construct the grid.
 * @param aLinksPattern the pattern (see below)
 *
 * Example: setLinks("1,2,3")
 * Result: [{url: "http://example.com/#1", title: "site#1"},
 *          {url: "http://example.com/#2", title: "site#2"}
 *          {url: "http://example.com/#3", title: "site#3"}]
 */
function setLinks(aLinks) {
  let links = aLinks;

  if (typeof links == "string") {
    links = aLinks.split(/\s*,\s*/).map(function (id) {
      return {url: "http://example.com/#" + id, title: "site#" + id};
    });
  }

  clearHistory();
  fillHistory(links, function () {
    NewTabUtils.links.populateCache(function () {
      NewTabUtils.allPages.update();
      TestRunner.next();
    }, true);
  });
}

function clearHistory() {
  PlacesUtils.history.removeAllPages();
}

function fillHistory(aLinks, aCallback) {
  let numLinks = aLinks.length;
  let transitionLink = Ci.nsINavHistoryService.TRANSITION_LINK;

  for (let link of aLinks.reverse()) {
    let place = {
      uri: makeURI(link.url),
      title: link.title,
      visits: [{visitDate: Date.now() * 1000, transitionType: transitionLink}]
    };

    PlacesUtils.asyncHistory.updatePlaces(place, {
      handleError: function () ok(false, "couldn't add visit to history"),
      handleResult: function () {},
      handleCompletion: function () {
        if (--numLinks == 0)
          aCallback();
      }
    });
  }
}

/**
 * Allows to specify the list of pinned links (that have a fixed position in
 * the grid.
 * @param aLinksPattern the pattern (see below)
 *
 * Example: setPinnedLinks("3,,1")
 * Result: 'http://example.com/#3' is pinned in the first cell. 'http://example.com/#1' is
 *         pinned in the third cell.
 */
function setPinnedLinks(aLinks) {
  let links = aLinks;

  if (typeof links == "string") {
    links = aLinks.split(/\s*,\s*/).map(function (id) {
      if (id)
        return {url: "http://example.com/#" + id, title: "site#" + id};
    });
  }

  storage.setItem("pinnedLinks", JSON.stringify(links));
  NewTabUtils.pinnedLinks.resetCache();
  NewTabUtils.allPages.update();
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

    if (NewTabUtils.allPages.enabled) {
      // Continue when the link cache has been populated.
      NewTabUtils.links.populateCache(function () {
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
 * Result: We expect the first cell to contain the pinned site 'http://example.com/#3'.
 *         The second cell contains 'http://example.com/#2'. The third cell is empty.
 *         The fourth cell contains the pinned site 'http://example.com/#4'.
 */
function checkGrid(aSitesPattern, aSites) {
  let length = aSitesPattern.split(",").length;
  let sites = (aSites || getGrid().sites).slice(0, length);
  let current = sites.map(function (aSite) {
    if (!aSite)
      return "";

    let pinned = aSite.isPinned();
    let pinButton = aSite.node.querySelector(".newtab-control-pin");
    let hasPinnedAttr = pinButton.hasAttribute("pinned");

    if (pinned != hasPinnedAttr)
      ok(false, "invalid state (site.isPinned() != site[pinned])");

    return aSite.url.replace(/^http:\/\/example\.com\/#(\d+)$/, "$1") + (pinned ? "p" : "");
  });

  is(current, aSitesPattern, "grid status = " + aSitesPattern);
}

/**
 * Blocks a site from the grid.
 * @param aIndex The cell index.
 */
function blockCell(aIndex) {
  whenPagesUpdated();
  getCell(aIndex).site.block();
}

/**
 * Pins a site on a given position.
 * @param aIndex The cell index.
 * @param aPinIndex The index the defines where the site should be pinned.
 */
function pinCell(aIndex, aPinIndex) {
  getCell(aIndex).site.pin(aPinIndex);
}

/**
 * Unpins the given cell's site.
 * @param aIndex The cell index.
 */
function unpinCell(aIndex) {
  whenPagesUpdated();
  getCell(aIndex).site.unpin();
}

/**
 * Simulates a drop and drop operation.
 * @param aDropIndex The cell index of the drop target.
 * @param aDragIndex The cell index containing the dragged site (optional).
 */
function simulateDrop(aDropIndex, aDragIndex) {
  let draggedSite;
  let {gDrag: drag, gDrop: drop} = getContentWindow();
  let event = createDragEvent("drop", "http://example.com/#99\nblank");

  if (typeof aDragIndex != "undefined")
    draggedSite = getCell(aDragIndex).site;

  if (draggedSite)
    drag.start(draggedSite, event);

  whenPagesUpdated();
  drop.drop(getCell(aDropIndex), event);

  if (draggedSite)
    drag.end(draggedSite);
}

/**
 * Sends a custom drag event to a given DOM element.
 * @param aEventType The drag event's type.
 * @param aTarget The DOM element that the event is dispatched to.
 * @param aData The event's drag data (optional).
 */
function sendDragEvent(aEventType, aTarget, aData) {
  let event = createDragEvent(aEventType, aData);
  let ifaceReq = getContentWindow().QueryInterface(Ci.nsIInterfaceRequestor);
  let windowUtils = ifaceReq.getInterface(Ci.nsIDOMWindowUtils);
  windowUtils.dispatchDOMEventViaPresShell(aTarget, event, true);
}

/**
 * Creates a custom drag event.
 * @param aEventType The drag event's type.
 * @param aData The event's drag data (optional).
 * @return The drag event.
 */
function createDragEvent(aEventType, aData) {
  let dataTransfer = {
    mozUserCancelled: false,
    setData: function () null,
    setDragImage: function () null,
    getData: function () aData,

    types: {
      contains: function (aType) aType == "text/x-moz-url"
    },

    mozGetDataAt: function (aType, aIndex) {
      if (aIndex || aType != "text/x-moz-url")
        return null;

      return aData;
    }
  };

  let event = getContentDocument().createEvent("DragEvents");
  event.initDragEvent(aEventType, true, true, getContentWindow(), 0, 0, 0, 0, 0,
                      false, false, false, false, 0, null, dataTransfer);

  return event;
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
