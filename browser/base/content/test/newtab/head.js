/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_NEWTAB_ENABLED = "browser.newtabpage.enabled";
const PREF_NEWTAB_DIRECTORYSOURCE = "browser.newtabpage.directory.source";

Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, true);

var tmp = {};
Cu.import("resource://gre/modules/Promise.jsm", tmp);
Cu.import("resource://gre/modules/NewTabUtils.jsm", tmp);
Cu.import("resource:///modules/DirectoryLinksProvider.jsm", tmp);
Cu.import("resource://testing-common/PlacesTestUtils.jsm", tmp);
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://browser/content/sanitize.js", tmp);
Cu.import("resource://gre/modules/Timer.jsm", tmp);
var {Promise, NewTabUtils, Sanitizer, clearTimeout, setTimeout, DirectoryLinksProvider, PlacesTestUtils} = tmp;

var uri = Services.io.newURI("about:newtab", null, null);
var principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});

var isMac = ("nsILocalFileMac" in Ci);
var isLinux = ("@mozilla.org/gnome-gconf-service;1" in Cc);
var isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);
var gWindow = window;

// Default to dummy/empty directory links
var gDirectorySource = 'data:application/json,{"test":1}';
var gOrigDirectorySource;

// The tests assume all 3 rows and all 3 columns of sites are shown, but the
// window may be too small to actually show everything.  Resize it if necessary.
var requiredSize = {};
requiredSize.innerHeight =
  40 + 32 + // undo container + bottom margin
  44 + 32 + // search bar + bottom margin
  (3 * (180 + 32)) + // 3 rows * (tile height + title and bottom margin)
  100; // breathing room
requiredSize.innerWidth =
  (3 * (290 + 20)) + // 3 cols * (tile width + side margins)
  100; // breathing room

var oldSize = {};
Object.keys(requiredSize).forEach(prop => {
  info([prop, gBrowser.contentWindow[prop], requiredSize[prop]]);
  if (gBrowser.contentWindow[prop] < requiredSize[prop]) {
    oldSize[prop] = gBrowser.contentWindow[prop];
    info("Changing browser " + prop + " from " + oldSize[prop] + " to " +
         requiredSize[prop]);
    gBrowser.contentWindow[prop] = requiredSize[prop];
  }
});

var screenHeight = {};
var screenWidth = {};
Cc["@mozilla.org/gfx/screenmanager;1"].
  getService(Ci.nsIScreenManager).
  primaryScreen.
  GetAvailRectDisplayPix({}, {}, screenWidth, screenHeight);
screenHeight = screenHeight.value;
screenWidth = screenWidth.value;

if (screenHeight < gBrowser.contentWindow.outerHeight) {
  info("Warning: Browser outer height is now " +
       gBrowser.contentWindow.outerHeight + ", which is larger than the " +
       "available screen height, " + screenHeight +
       ". That may cause problems.");
}

if (screenWidth < gBrowser.contentWindow.outerWidth) {
  info("Warning: Browser outer width is now " +
       gBrowser.contentWindow.outerWidth + ", which is larger than the " +
       "available screen width, " + screenWidth +
       ". That may cause problems.");
}

registerCleanupFunction(function () {
  while (gWindow.gBrowser.tabs.length > 1)
    gWindow.gBrowser.removeTab(gWindow.gBrowser.tabs[1]);

  Object.keys(oldSize).forEach(prop => {
    if (oldSize[prop]) {
      gBrowser.contentWindow[prop] = oldSize[prop];
    }
  });

  // Stop any update timers to prevent unexpected updates in later tests
  let timer = NewTabUtils.allPages._scheduleUpdateTimeout;
  if (timer) {
    clearTimeout(timer);
    delete NewTabUtils.allPages._scheduleUpdateTimeout;
  }

  Services.prefs.clearUserPref(PREF_NEWTAB_ENABLED);
  Services.prefs.setCharPref(PREF_NEWTAB_DIRECTORYSOURCE, gOrigDirectorySource);

  return watchLinksChangeOnce();
});

/**
 * Resolves promise when directory links are downloaded and written to disk
 */
function watchLinksChangeOnce() {
  let deferred = Promise.defer();
  let observer = {
    onManyLinksChanged: () => {
      DirectoryLinksProvider.removeObserver(observer);
      deferred.resolve();
    }
  };
  observer.onDownloadFail = observer.onManyLinksChanged;
  DirectoryLinksProvider.addObserver(observer);
  return deferred.promise;
};

/**
 * Provide the default test function to start our test runner.
 *
 * We need different code paths for tests that are still wired for
 * `TestRunner` and tests that have been ported to `add_task` as
 * we cannot have both in the same file.
 */
function isTestPortedToAddTask() {
  return gTestPath.endsWith("browser_newtab_bug722273.js");
}
if (!isTestPortedToAddTask()) {
  this.test = function() {
    waitForExplicitFinish();
    // start TestRunner.run() after directory links is downloaded and written to disk
    watchLinksChangeOnce().then(() => {
      // Wait for hidden page to update with the desired links
      whenPagesUpdated(() => TestRunner.run(), true);
    });

    // Save the original directory source (which is set globally for tests)
    gOrigDirectorySource = Services.prefs.getCharPref(PREF_NEWTAB_DIRECTORYSOURCE);
    Services.prefs.setCharPref(PREF_NEWTAB_DIRECTORYSOURCE, gDirectorySource);
  }
} else {
  add_task(function* setup() {
    registerCleanupFunction(function() {
      return new Promise(resolve => {
        function cleanupAndFinish() {
          PlacesTestUtils.clearHistory().then(() => {
            whenPagesUpdated(resolve);
            NewTabUtils.restore();
          });
        }

        let callbacks = NewTabUtils.links._populateCallbacks;
        let numCallbacks = callbacks.length;

        if (numCallbacks)
          callbacks.splice(0, numCallbacks, cleanupAndFinish);
        else
          cleanupAndFinish();
      });
    });

    let promiseReady = Task.spawn(function*() {
      yield watchLinksChangeOnce();
      yield new Promise(resolve => whenPagesUpdated(resolve, true));
    });

    // Save the original directory source (which is set globally for tests)
    gOrigDirectorySource = Services.prefs.getCharPref(PREF_NEWTAB_DIRECTORYSOURCE);
    Services.prefs.setCharPref(PREF_NEWTAB_DIRECTORYSOURCE, gDirectorySource);
    yield promiseReady;
  });
}

/**
 * The test runner that controls the execution flow of our tests.
 */
var TestRunner = {
  /**
   * Starts the test runner.
   */
  run: function () {
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
      PlacesTestUtils.clearHistory().then(() => {
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
 * Example: setLinks("-1,0,1,2,3")
 * Result: [{url: "http://example.com/", title: "site#-1"},
 *          {url: "http://example0.com/", title: "site#0"},
 *          {url: "http://example1.com/", title: "site#1"},
 *          {url: "http://example2.com/", title: "site#2"},
 *          {url: "http://example3.com/", title: "site#3"}]
 */
function setLinks(aLinks, aCallback = TestRunner.next) {
  let links = aLinks;

  if (typeof links == "string") {
    links = aLinks.split(/\s*,\s*/).map(function (id) {
      return {url: "http://example" + (id != "-1" ? id : "") + ".com/",
              title: "site#" + id};
    });
  }

  // Call populateCache() once to make sure that all link fetching that is
  // currently in progress has ended. We clear the history, fill it with the
  // given entries and call populateCache() now again to make sure the cache
  // has the desired contents.
  NewTabUtils.links.populateCache(function () {
    PlacesTestUtils.clearHistory().then(() => {
      fillHistory(links, function () {
        NewTabUtils.links.populateCache(function () {
          NewTabUtils.allPages.update();
          aCallback();
        }, true);
      });
    });
  });
}

function fillHistory(aLinks, aCallback = TestRunner.next) {
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
      handleError: () => ok(false, "couldn't add visit to history"),
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
 * Result: 'http://example3.com/' is pinned in the first cell. 'http://example1.com/' is
 *         pinned in the third cell.
 */
function setPinnedLinks(aLinks) {
  let links = aLinks;

  if (typeof links == "string") {
    links = aLinks.split(/\s*,\s*/).map(function (id) {
      if (id)
        return {url: "http://example" + (id != "-1" ? id : "") + ".com/",
                title: "site#" + id,
                type: "history"};
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
 * Wait until a given condition becomes true.
 */
function waitForCondition(aConditionFn, aMaxTries=50, aCheckInterval=100) {
  return new Promise((resolve, reject) => {
    let tries = 0;

    function tryNow() {
      tries++;

      if (aConditionFn()) {
        resolve();
      } else if (tries < aMaxTries) {
        tryAgain();
      } else {
        reject("Condition timed out: " + aConditionFn.toSource());
      }
    }

    function tryAgain() {
      setTimeout(tryNow, aCheckInterval);
    }

    tryAgain();
  });
}

/**
 * Creates a new tab containing 'about:newtab'.
 */
function addNewTabPageTab() {
  addNewTabPageTabPromise().then(TestRunner.next);
}

function addNewTabPageTabPromise() {
  let deferred = Promise.defer();

  let tab = gWindow.gBrowser.selectedTab = gWindow.gBrowser.addTab("about:newtab");
  let browser = tab.linkedBrowser;

  function whenNewTabLoaded() {
    if (NewTabUtils.allPages.enabled) {
      // Continue when the link cache has been populated.
      NewTabUtils.links.populateCache(function () {
        deferred.resolve(whenSearchInitDone());
      });
    } else {
      deferred.resolve();
    }
  }

  // Wait for the new tab page to be loaded.
  waitForBrowserLoad(browser, function () {
    // Wait for the document to become visible in case it was preloaded.
    waitForCondition(() => !browser.contentDocument.hidden).then(whenNewTabLoaded);
  });

  return deferred.promise;
}

function waitForBrowserLoad(browser, callback = TestRunner.next) {
  if (browser.contentDocument.readyState == "complete") {
    executeSoon(callback);
    return;
  }

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    executeSoon(callback);
  }, true);
}

/**
 * Compares the current grid arrangement with the given pattern.
 * @param the pattern (see below)
 * @param the array of sites to compare with (optional)
 *
 * Example: checkGrid("3p,2,,1p")
 * Result: We expect the first cell to contain the pinned site 'http://example3.com/'.
 *         The second cell contains 'http://example2.com/'. The third cell is empty.
 *         The fourth cell contains the pinned site 'http://example4.com/'.
 */
function checkGrid(aSitesPattern, aSites) {
  let length = aSitesPattern.split(",").length;
  let sites = (aSites || getGrid().sites).slice(0, length);
  let current = sites.map(function (aSite) {
    if (!aSite)
      return "";

    let pinned = aSite.isPinned();
    let hasPinnedAttr = aSite.node.hasAttribute("pinned");

    if (pinned != hasPinnedAttr)
      ok(false, "invalid state (site.isPinned() != site[pinned])");

    return aSite.url.replace(/^http:\/\/example(\d+)\.com\/$/, "$1") + (pinned ? "p" : "");
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
  // The implementation of this function varies by platform because each
  // platform has particular quirks that we need to deal with

  if (isMac) {
    // On OS X once the drag starts, Cocoa manages the drag session and
    // gives us a limited amount of time to complete the drag operation. In
    // some cases as soon as the first mouse-move event is received (the one
    // that starts the drag session), Cocoa becomes blind to subsequent mouse
    // events and completes the drag session all by itself. Therefore it is
    // important that the first mouse-move we send is already positioned at
    // the destination.
    synthesizeNativeMouseLDown(aSource);
    synthesizeNativeMouseDrag(aDest);
    // In some tests, aSource and aDest are at the same position, so to ensure
    // a drag session is created (instead of it just turning into a click) we
    // move the mouse 10 pixels away and then back.
    synthesizeNativeMouseDrag(aDest, 10);
    synthesizeNativeMouseDrag(aDest);
    // Finally, release the drag and have it run the callback when done.
    synthesizeNativeMouseLUp(aDest).then(aCallback, Cu.reportError);
  } else if (isWindows) {
    // on Windows once the drag is initiated, Windows doesn't spin our
    // message loop at all, so with async event synthesization the async
    // messages never get processed while a drag is in progress. So if
    // we did a mousedown followed by a mousemove, we would never be able
    // to successfully dispatch the mouseup. Instead, we just skip the move
    // entirely, so and just generate the up at the destination. This way
    // Windows does the drag and also terminates it right away. Note that
    // this only works for tests where aSource and aDest are sufficiently
    // far to trigger a drag, otherwise it may just end up doing a click.
    synthesizeNativeMouseLDown(aSource);
    synthesizeNativeMouseLUp(aDest).then(aCallback, Cu.reportError);
  } else if (isLinux) {
    // Start by pressing the left mouse button.
    synthesizeNativeMouseLDown(aSource);

    // Move the mouse in 5px steps until the drag operation starts.
    // Note that we need to do this with pauses in between otherwise the
    // synthesized events get coalesced somewhere in the guts of GTK. In order
    // to successfully initiate a drag session in the case where aSource and
    // aDest are at the same position, we synthesize a bunch of drags until
    // we know the drag session has started, and then move to the destination.
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
    // Note that we need to actually wait for the dragenter event here, because
    // the mousemove synthesization is "more async" than the mouseup
    // synthesization - they use different gdk APIs. If we don't wait, the
    // up could get processed before the moves, dropping the item in the
    // wrong position.
    aDest.addEventListener("dragenter", function onDragEnter() {
      aDest.removeEventListener("dragenter", onDragEnter);

      // Finish the drop operation.
      synthesizeNativeMouseLUp(aDest).then(aCallback, Cu.reportError);
    });
  } else {
    throw "Unsupported platform";
  }
}

/**
 * Helper function that creates a temporary iframe in the about:newtab
 * document. This will contain a link we can drag to the test the dropping
 * of links from external documents.
 */
function createExternalDropIframe() {
  const url = "data:text/html;charset=utf-8," +
              "<a id='link' href='http://example99.com/'>link</a>";

  let deferred = Promise.defer();
  let doc = getContentDocument();
  let iframe = doc.createElement("iframe");
  iframe.setAttribute("src", url);
  iframe.style.width = "50px";
  iframe.style.height = "50px";
  iframe.style.position = "absolute";
  iframe.style.zIndex = 50;

  // the frame has to be attached to a visible element
  let margin = doc.getElementById("newtab-search-container");
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
  return synthesizeNativeMouseEvent(aElement, msg);
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
  return new Promise((resolve, reject) => {
    let rect = aElement.getBoundingClientRect();
    let win = aElement.ownerDocument.defaultView;
    let x = aOffsetX + win.mozInnerScreenX + rect.left + rect.width / 2;
    let y = aOffsetY + win.mozInnerScreenY + rect.top + rect.height / 2;

    let utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils);

    let scale = utils.screenPixelsPerCSSPixel;
    let observer = {
      observe: function(aSubject, aTopic, aData) {
        if (aTopic == "mouseevent") {
          resolve();
        }
      }
    };
    utils.sendNativeMouseEvent(x * scale, y * scale, aMsg, 0, null, observer);
  });
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
 */
function whenPagesUpdated(aCallback = TestRunner.next) {
  let page = {
    observe: _ => _,

    update() {
      NewTabUtils.allPages.unregister(this);
      executeSoon(aCallback);
    }
  };

  NewTabUtils.allPages.register(page);
  registerCleanupFunction(function () {
    NewTabUtils.allPages.unregister(page);
  });
}

/**
 * Waits for the response to the page's initial search state request.
 */
function whenSearchInitDone() {
  let deferred = Promise.defer();
  let searchController = getContentWindow().gSearch._contentSearchController;
  if (searchController.defaultEngine) {
    return Promise.resolve();
  }
  let eventName = "ContentSearchService";
  getContentWindow().addEventListener(eventName, function onEvent(event) {
    if (event.detail.type == "State") {
      getContentWindow().removeEventListener(eventName, onEvent);
      // Wait for the search controller to receive the event, then resolve.
      let resolver = function() {
        if (searchController.defaultEngine) {
          deferred.resolve();
          return;
        }
        executeSoon(resolver);
      }
      executeSoon(resolver);
    }
  });
  return deferred.promise;
}

/**
 * Changes the newtab customization option and waits for the panel to open and close
 *
 * @param {string} aTheme
 *        Can be any of("blank"|"classic"|"enhanced")
 */
function customizeNewTabPage(aTheme) {
  let promise = ContentTask.spawn(gBrowser.selectedBrowser, aTheme, function*(aTheme) {

    let document = content.document;
    let panel = document.getElementById("newtab-customize-panel");
    let customizeButton = document.getElementById("newtab-customize-button");

    function panelOpened(opened) {
      return new Promise( (resolve) => {
        let options = {attributes: true, oldValue: true};
        let observer = new content.MutationObserver(function(mutations) {
          mutations.forEach(function(mutation) {
            document.getElementById("newtab-customize-" + aTheme).click();
            observer.disconnect();
            if (opened == panel.hasAttribute("open")) {
              resolve();
            }
          });
        });
        observer.observe(panel, options);
      });
    }

    let opened = panelOpened(true);
    customizeButton.click();
    yield opened;

    let closed = panelOpened(false);
    customizeButton.click();
    yield closed;
  });

  promise.then(TestRunner.next);
}

/**
 * Reports presence of a scrollbar
 */
function hasScrollbar() {
  let docElement = getContentDocument().documentElement;
  return docElement.scrollHeight > docElement.clientHeight;
}
