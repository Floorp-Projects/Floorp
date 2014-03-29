/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_NEWTAB_ENABLED = "browser.newtabpage.enabled";
const PREF_NEWTAB_DIRECTORYSOURCE = "browser.newtabpage.directorySource";

Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, true);
// start with no directory links by default
Services.prefs.setCharPref(PREF_NEWTAB_DIRECTORYSOURCE, "data:application/json,{}");

let tmp = {};
Cu.import("resource://gre/modules/Promise.jsm", tmp);
Cu.import("resource://gre/modules/NewTabUtils.jsm", tmp);
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://browser/content/sanitize.js", tmp);
let {Promise, NewTabUtils, Sanitizer} = tmp;

let uri = Services.io.newURI("about:newtab", null, null);
let principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(uri);

let isMac = ("nsILocalFileMac" in Ci);
let isLinux = ("@mozilla.org/gnome-gconf-service;1" in Cc);
let isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);
let gWindow = window;

registerCleanupFunction(function () {
  while (gWindow.gBrowser.tabs.length > 1)
    gWindow.gBrowser.removeTab(gWindow.gBrowser.tabs[1]);

  Services.prefs.clearUserPref(PREF_NEWTAB_ENABLED);
  Services.prefs.clearUserPref(PREF_NEWTAB_DIRECTORYSOURCE);
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
      clearHistory(function () {
        whenPagesUpdated(finish);
        NewTabUtils.restore();
      });
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
  return gWindow.gBrowser.selectedBrowser.contentWindow;
}

/**
 * Returns the selected tab's content document.
 * @return The content document.
 */
function getContentDocument() {
  return gWindow.gBrowser.selectedBrowser.contentDocument;
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

  // Call populateCache() once to make sure that all link fetching that is
  // currently in progress has ended. We clear the history, fill it with the
  // given entries and call populateCache() now again to make sure the cache
  // has the desired contents.
  NewTabUtils.links.populateCache(function () {
    clearHistory(function () {
      fillHistory(links, function () {
        NewTabUtils.links.populateCache(function () {
          NewTabUtils.allPages.update();
          TestRunner.next();
        }, true);
      });
    });
  });
}

function clearHistory(aCallback) {
  Services.obs.addObserver(function observe(aSubject, aTopic, aData) {
    Services.obs.removeObserver(observe, aTopic);
    executeSoon(aCallback);
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  PlacesUtils.history.removeAllPages();
}

function fillHistory(aLinks, aCallback) {
  let numLinks = aLinks.length;
  if (!numLinks) {
    if (aCallback)
      executeSoon(aCallback);
    return;
  }

  let transitionLink = Ci.nsINavHistoryService.TRANSITION_LINK;

  // Important: To avoid test failures due to clock jitter on Windows XP, call
  // Date.now() once here, not each time through the loop.
  let now = Date.now() * 1000;

  for (let i = 0; i < aLinks.length; i++) {
    let link = aLinks[i];
    let place = {
      uri: makeURI(link.url),
      title: link.title,
      // Links are secondarily sorted by visit date descending, so decrease the
      // visit date as we progress through the array so that links appear in the
      // grid in the order they're present in the array.
      visits: [{visitDate: now - i, transitionType: transitionLink}]
    };

    PlacesUtils.asyncHistory.updatePlaces(place, {
      handleError: function () ok(false, "couldn't add visit to history"),
      handleResult: function () {},
      handleCompletion: function () {
        if (--numLinks == 0 && aCallback)
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

  let string = Cc["@mozilla.org/supports-string;1"]
                 .createInstance(Ci.nsISupportsString);
  string.data = JSON.stringify(links);
  Services.prefs.setComplexValue("browser.newtabpage.pinned",
                                 Ci.nsISupportsString, string);

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
  let tab = gWindow.gBrowser.selectedTab = gWindow.gBrowser.addTab("about:newtab");
  let browser = tab.linkedBrowser;

  function whenNewTabLoaded() {
    if (NewTabUtils.allPages.enabled) {
      // Continue when the link cache has been populated.
      NewTabUtils.links.populateCache(function () {
        executeSoon(TestRunner.next);
      });
    } else {
      // It's important that we call next() asynchronously.
      // 'yield addNewTabPageTab()' would fail if next() is called
      // synchronously because the iterator is already executing.
      executeSoon(TestRunner.next);
    }
  }

  // The new tab page might have been preloaded in the background.
  if (browser.contentDocument.readyState == "complete") {
    whenNewTabLoaded();
    return;
  }

  // Wait for the new tab page to be loaded.
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    whenNewTabLoaded();
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
 * Simulates a drag and drop operation.
 * @param aSourceIndex The cell index containing the dragged site.
 * @param aDestIndex The cell index of the drop target.
 */
function simulateDrop(aSourceIndex, aDestIndex) {
  let src = getCell(aSourceIndex).site.node;
  let dest = getCell(aDestIndex).node;

  // Drop 'src' onto 'dest' and continue testing when all newtab
  // pages have been updated (i.e. the drop operation is completed).
  startAndCompleteDragOperation(src, dest, whenPagesUpdated);
}

/**
 * Simulates a drag and drop operation. Instead of rearranging a site that is
 * is already contained in the newtab grid, this is used to simulate dragging
 * an external link onto the grid e.g. the text from the URL bar.
 * @param aDestIndex The cell index of the drop target.
 */
function simulateExternalDrop(aDestIndex) {
  let dest = getCell(aDestIndex).node;

  // Create an iframe that contains the external link we'll drag.
  createExternalDropIframe().then(iframe => {
    let link = iframe.contentDocument.getElementById("link");

    // Drop 'link' onto 'dest'.
    startAndCompleteDragOperation(link, dest, () => {
      // Wait until the drop operation is complete
      // and all newtab pages have been updated.
      whenPagesUpdated(() => {
        // Clean up and remove the iframe.
        iframe.remove();
        // Continue testing.
        TestRunner.next();
      });
    });
  });
}

/**
 * Starts and complete a drag-and-drop operation.
 * @param aSource The node that is being dragged.
 * @param aDest The node we're dragging aSource onto.
 * @param aCallback The function that is called when we're done.
 */
function startAndCompleteDragOperation(aSource, aDest, aCallback) {
  // Start by pressing the left mouse button.
  synthesizeNativeMouseLDown(aSource);

  // Move the mouse in 5px steps until the drag operation starts.
  let offset = 0;
  let interval = setInterval(() => {
    synthesizeNativeMouseDrag(aSource, offset += 5);
  }, 10);

  // When the drag operation has started we'll move
  // the dragged element to its target position.
  aSource.addEventListener("dragstart", function onDragStart() {
    aSource.removeEventListener("dragstart", onDragStart);
    clearInterval(interval);

    // Place the cursor above the drag target.
    synthesizeNativeMouseMove(aDest);
  });

  // As soon as the dragged element hovers the target, we'll drop it.
  aDest.addEventListener("dragenter", function onDragEnter() {
    aDest.removeEventListener("dragenter", onDragEnter);

    // Finish the drop operation.
    synthesizeNativeMouseLUp(aDest);
    aCallback();
  });
}

/**
 * Helper function that creates a temporary iframe in the about:newtab
 * document. This will contain a link we can drag to the test the dropping
 * of links from external documents.
 */
function createExternalDropIframe() {
  const url = "data:text/html;charset=utf-8," +
              "<a id='link' href='http://example.com/%2399'>link</a>";

  let deferred = Promise.defer();
  let doc = getContentDocument();
  let iframe = doc.createElement("iframe");
  iframe.setAttribute("src", url);
  iframe.style.width = "50px";
  iframe.style.height = "50px";

  let margin = doc.getElementById("newtab-margin-top");
  margin.appendChild(iframe);

  iframe.addEventListener("load", function onLoad() {
    iframe.removeEventListener("load", onLoad);
    executeSoon(() => deferred.resolve(iframe));
  });

  return deferred.promise;
}

/**
 * Fires a synthetic 'mousedown' event on the current about:newtab page.
 * @param aElement The element used to determine the cursor position.
 */
function synthesizeNativeMouseLDown(aElement) {
  if (isLinux) {
    let win = aElement.ownerDocument.defaultView;
    EventUtils.synthesizeMouseAtCenter(aElement, {type: "mousedown"}, win);
  } else {
    let msg = isWindows ? 2 : 1;
    synthesizeNativeMouseEvent(aElement, msg);
  }
}

/**
 * Fires a synthetic 'mouseup' event on the current about:newtab page.
 * @param aElement The element used to determine the cursor position.
 */
function synthesizeNativeMouseLUp(aElement) {
  let msg = isWindows ? 4 : (isMac ? 2 : 7);
  synthesizeNativeMouseEvent(aElement, msg);
}

/**
 * Fires a synthetic mouse drag event on the current about:newtab page.
 * @param aElement The element used to determine the cursor position.
 * @param aOffsetX The left offset that is added to the position.
 */
function synthesizeNativeMouseDrag(aElement, aOffsetX) {
  let msg = isMac ? 6 : 1;
  synthesizeNativeMouseEvent(aElement, msg, aOffsetX);
}

/**
 * Fires a synthetic 'mousemove' event on the current about:newtab page.
 * @param aElement The element used to determine the cursor position.
 */
function synthesizeNativeMouseMove(aElement) {
  let msg = isMac ? 5 : 1;
  synthesizeNativeMouseEvent(aElement, msg);
}

/**
 * Fires a synthetic mouse event on the current about:newtab page.
 * @param aElement The element used to determine the cursor position.
 * @param aOffsetX The left offset that is added to the position (optional).
 * @param aOffsetY The top offset that is added to the position (optional).
 */
function synthesizeNativeMouseEvent(aElement, aMsg, aOffsetX = 0, aOffsetY = 0) {
  let rect = aElement.getBoundingClientRect();
  let win = aElement.ownerDocument.defaultView;
  let x = aOffsetX + win.mozInnerScreenX + rect.left + rect.width / 2;
  let y = aOffsetY + win.mozInnerScreenY + rect.top + rect.height / 2;

  let utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindowUtils);

  let scale = utils.screenPixelsPerCSSPixel;
  utils.sendNativeMouseEvent(x * scale, y * scale, aMsg, 0, null);
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
  let dataTransfer = new (getContentWindow()).DataTransfer("dragstart", false);
  dataTransfer.mozSetDataAt("text/x-moz-url", aData, 0);
  let event = getContentDocument().createEvent("DragEvents");
  event.initDragEvent(aEventType, true, true, getContentWindow(), 0, 0, 0, 0, 0,
                      false, false, false, false, 0, null, dataTransfer);

  return event;
}

/**
 * Resumes testing when all pages have been updated.
 * @param aCallback Called when done. If not specified, TestRunner.next is used.
 * @param aOnlyIfHidden If true, this resumes testing only when an update that
 *                      applies to pre-loaded, hidden pages is observed.  If
 *                      false, this resumes testing when any update is observed.
 */
function whenPagesUpdated(aCallback, aOnlyIfHidden=false) {
  let page = {
    update: function (onlyIfHidden=false) {
      if (onlyIfHidden == aOnlyIfHidden) {
        NewTabUtils.allPages.unregister(this);
        executeSoon(aCallback || TestRunner.next);
      }
    }
  };

  NewTabUtils.allPages.register(page);
  registerCleanupFunction(function () {
    NewTabUtils.allPages.unregister(page);
  });
}
