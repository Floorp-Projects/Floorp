/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_NEWTAB_ENABLED = "browser.newtabpage.enabled";
const PREF_NEWTAB_DIRECTORYSOURCE = "browser.newtabpage.directory.source";

Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, true);

var tmp = {};
Cu.import("resource://gre/modules/NewTabUtils.jsm", tmp);
Cu.import("resource:///modules/DirectoryLinksProvider.jsm", tmp);
Cu.import("resource://testing-common/PlacesTestUtils.jsm", tmp);
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://browser/content/sanitize.js", tmp);
var {NewTabUtils, Sanitizer, DirectoryLinksProvider, PlacesTestUtils} = tmp;

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

function pushPrefs(...aPrefs) {
  return new Promise(resolve =>
                     SpecialPowers.pushPrefEnv({"set": aPrefs}, resolve));
}

/**
 * Resolves promise when directory links are downloaded and written to disk
 */
function watchLinksChangeOnce() {
  return new Promise(resolve => {
    let observer = {
      onManyLinksChanged: () => {
        DirectoryLinksProvider.removeObserver(observer);
        resolve();
      }
    };
    observer.onDownloadFail = observer.onManyLinksChanged;
    DirectoryLinksProvider.addObserver(observer);
  });
}

add_task(function* setup() {
  registerCleanupFunction(function() {
    return new Promise(resolve => {
      function cleanupAndFinish() {
        PlacesTestUtils.clearHistory().then(() => {
          whenPagesUpdated().then(resolve);
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
    yield whenPagesUpdated();
  });

  // Save the original directory source (which is set globally for tests)
  gOrigDirectorySource = Services.prefs.getCharPref(PREF_NEWTAB_DIRECTORYSOURCE);
  Services.prefs.setCharPref(PREF_NEWTAB_DIRECTORYSOURCE, gDirectorySource);
  yield promiseReady;
});

/** Perform an action on a cell within the newtab page.
  * @param aIndex index of cell
  * @param aFn function to call in child process or tab.
  * @returns result of calling the function.
  */
function performOnCell(aIndex, aFn) {
  return ContentTask.spawn(gWindow.gBrowser.selectedBrowser,
                           { index: aIndex, fn: aFn.toString() }, function* (args) {
    let cell = content.gGrid.cells[args.index];
    return eval("(" + args.fn + ")(cell)");
  });
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
function setLinks(aLinks) {
  return new Promise(resolve => {
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
        fillHistory(links).then(() => {
          NewTabUtils.links.populateCache(function () {
            NewTabUtils.allPages.update();
            resolve();
          }, true);
        });
      });
    });
  });
}

function fillHistory(aLinks) {
  return new Promise(resolve => {
    let numLinks = aLinks.length;
    if (!numLinks) {
      executeSoon(resolve);
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
          if (--numLinks == 0) {
            resolve();
          }
        }
      });
    }
  });
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
      return undefined;
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
  return new Promise(resolve => {
    whenPagesUpdated().then(resolve);
    NewTabUtils.restore();
  });
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
function* addNewTabPageTab() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gWindow.gBrowser, "about:newtab", false);
  let browser = tab.linkedBrowser;

  // Wait for the document to become visible in case it was preloaded.
  yield waitForCondition(() => !browser.contentDocument.hidden)

  yield new Promise(resolve => {
    if (NewTabUtils.allPages.enabled) {
      // Continue when the link cache has been populated.
      NewTabUtils.links.populateCache(function () {
        whenSearchInitDone().then(resolve);
      });
    } else {
      resolve();
    }
  });

  return tab;
}

/**
 * Compares the current grid arrangement with the given pattern.
 * @param the pattern (see below)
 *
 * Example: checkGrid("3p,2,,1p")
 * Result: We expect the first cell to contain the pinned site 'http://example3.com/'.
 *         The second cell contains 'http://example2.com/'. The third cell is empty.
 *         The fourth cell contains the pinned site 'http://example4.com/'.
 */
function* checkGrid(pattern) {
  let length = pattern.split(",").length;

  yield ContentTask.spawn(gWindow.gBrowser.selectedBrowser,
                          { length, pattern }, function* (args) {
    let grid = content.wrappedJSObject.gGrid;

    let sites = grid.sites.slice(0, args.length);
    let foundPattern = sites.map(function (aSite) {
      if (!aSite)
        return "";

      let pinned = aSite.isPinned();
      let hasPinnedAttr = aSite.node.hasAttribute("pinned");

      if (pinned != hasPinnedAttr)
        ok(false, "invalid state (site.isPinned() != site[pinned])");

      return aSite.url.replace(/^http:\/\/example(\d+)\.com\/$/, "$1") + (pinned ? "p" : "");
    });

    Assert.equal(foundPattern, args.pattern, "grid status = " + args.pattern);
  });
}

/**
 * Blocks a site from the grid.
 * @param aIndex The cell index.
 */
function blockCell(aIndex) {
  return new Promise(resolve => {
    whenPagesUpdated().then(resolve);
    performOnCell(aIndex, cell => {
      return cell.site.block();
    });
  });
}

/**
 * Pins a site on a given position.
 * @param aIndex The cell index.
 * @param aPinIndex The index the defines where the site should be pinned.
 */
function pinCell(aIndex) {
  performOnCell(aIndex, cell => {
    cell.site.pin();
  });
}

/**
 * Unpins the given cell's site.
 * @param aIndex The cell index.
 */
function unpinCell(aIndex) {
  return new Promise(resolve => {
    whenPagesUpdated().then(resolve);
    performOnCell(aIndex, cell => {
      cell.site.unpin();
    });
  });
}

/**
 * Simulates a drag and drop operation. Instead of rearranging a site that is
 * is already contained in the newtab grid, this is used to simulate dragging
 * an external link onto the grid e.g. the text from the URL bar.
 * @param aDestIndex The cell index of the drop target.
 */
function* simulateExternalDrop(aDestIndex) {
  let pagesUpdatedPromise = whenPagesUpdated();

  yield ContentTask.spawn(gWindow.gBrowser.selectedBrowser, aDestIndex, function*(dropIndex) {
    return new Promise(resolve => {
      const url = "data:text/html;charset=utf-8," +
                  "<a id='link' href='http://example99.com/'>link</a>";

      let doc = content.document;
      let iframe = doc.createElement("iframe");

      function iframeLoaded() {
        let link = iframe.contentDocument.getElementById("link");

        let dataTransfer = new iframe.contentWindow.DataTransfer("dragstart", false);
        dataTransfer.mozSetDataAt("text/x-moz-url", "http://example99.com/", 0);

        let event = content.document.createEvent("DragEvent");
        event.initDragEvent("drop", true, true, content, 0, 0, 0, 0, 0,
                            false, false, false, false, 0, null, dataTransfer);

        let target = content.gGrid.cells[dropIndex].node;
        target.dispatchEvent(event);

        iframe.remove();

        resolve();
      }

      iframe.addEventListener("load", function onLoad() {
        iframe.removeEventListener("load", onLoad);
        content.setTimeout(iframeLoaded, 0);
      });

      iframe.setAttribute("src", url);
      iframe.style.width = "50px";
      iframe.style.height = "50px";
      iframe.style.position = "absolute";
      iframe.style.zIndex = 50;

      // the frame has to be attached to a visible element
      let margin = doc.getElementById("newtab-search-container");
      margin.appendChild(iframe);
    });
  });

  yield pagesUpdatedPromise;
}

/**
 * Resumes testing when all pages have been updated.
 */
function whenPagesUpdated() {
  return new Promise(resolve => {
    let page = {
      observe: _ => _,

      update() {
        NewTabUtils.allPages.unregister(this);
        executeSoon(resolve);
      }
    };

    NewTabUtils.allPages.register(page);
    registerCleanupFunction(function () {
      NewTabUtils.allPages.unregister(page);
    });
  });
}

/**
 * Waits for the response to the page's initial search state request.
 */
function whenSearchInitDone() {
  return ContentTask.spawn(gWindow.gBrowser.selectedBrowser, {}, function*() {
    return new Promise(resolve => {
      if (content.gSearch) {
        let searchController = content.gSearch._contentSearchController;
        if (searchController.defaultEngine) {
          resolve();
          return;
        }
      }

      let eventName = "ContentSearchService";
      content.addEventListener(eventName, function onEvent(event) {
        if (event.detail.type == "State") {
          content.removeEventListener(eventName, onEvent);
          let resolver = function() {
            // Wait for the search controller to receive the event, then resolve.
            if (content.gSearch._contentSearchController.defaultEngine) {
              resolve();
              return;
            }
          }
          content.setTimeout(resolver, 0);
        }
      });
    });
  });
}

/**
 * Changes the newtab customization option and waits for the panel to open and close
 *
 * @param {string} aTheme
 *        Can be any of("blank"|"classic"|"enhanced")
 */
function customizeNewTabPage(aTheme) {
  return ContentTask.spawn(gWindow.gBrowser.selectedBrowser, aTheme, function*(aTheme) {

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
}

/**
 * Reports presence of a scrollbar
 */
function hasScrollbar() {
  return ContentTask.spawn(gWindow.gBrowser.selectedBrowser, {}, function* () {
    let docElement = content.document.documentElement;
    return docElement.scrollHeight > docElement.clientHeight;
  });
}
