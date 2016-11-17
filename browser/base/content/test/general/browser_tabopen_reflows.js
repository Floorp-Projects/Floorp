/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyGetter(this, "docShell", () => {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsIDocShell);
});

const EXPECTED_REFLOWS = [
  // tabbrowser.adjustTabstrip() call after tabopen animation has finished
  "adjustTabstrip@chrome://browser/content/tabbrowser.xml|" +
    "_handleNewTab@chrome://browser/content/tabbrowser.xml|" +
    "onxbltransitionend@chrome://browser/content/tabbrowser.xml|",

  // switching focus in updateCurrentBrowser() causes reflows
  "_adjustFocusAfterTabSwitch@chrome://browser/content/tabbrowser.xml|" +
    "updateCurrentBrowser@chrome://browser/content/tabbrowser.xml|" +
    "onselect@chrome://browser/content/browser.xul|",

  // switching focus in openLinkIn() causes reflows
  "openLinkIn@chrome://browser/content/utilityOverlay.js|" +
    "openUILinkIn@chrome://browser/content/utilityOverlay.js|" +
    "BrowserOpenTab@chrome://browser/content/browser.js|",

  // accessing element.scrollPosition in _fillTrailingGap() flushes layout
  "get_scrollPosition@chrome://global/content/bindings/scrollbox.xml|" +
    "_fillTrailingGap@chrome://browser/content/tabbrowser.xml|" +
    "_handleNewTab@chrome://browser/content/tabbrowser.xml|" +
    "onxbltransitionend@chrome://browser/content/tabbrowser.xml|",

  // SessionStore.getWindowDimensions()
  "ssi_getWindowDimension@resource:///modules/sessionstore/SessionStore.jsm|" +
    "ssi_updateWindowFeatures/<@resource:///modules/sessionstore/SessionStore.jsm|" +
    "ssi_updateWindowFeatures@resource:///modules/sessionstore/SessionStore.jsm|" +
    "ssi_collectWindowData@resource:///modules/sessionstore/SessionStore.jsm|",

  // selection change notification may cause querying the focused editor content
  // by IME and that will cause reflow.
  "select@chrome://global/content/bindings/textbox.xml|" +
    "focusAndSelectUrlBar@chrome://browser/content/browser.js|" +
    "openLinkIn@chrome://browser/content/utilityOverlay.js|" +
    "openUILinkIn@chrome://browser/content/utilityOverlay.js|" +
    "BrowserOpenTab@chrome://browser/content/browser.js|",

];

const PREF_PRELOAD = "browser.newtab.preload";
const PREF_NEWTAB_DIRECTORYSOURCE = "browser.newtabpage.directory.source";

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when opening new tabs.
 */
add_task(function*() {
  let DirectoryLinksProvider = Cu.import("resource:///modules/DirectoryLinksProvider.jsm", {}).DirectoryLinksProvider;
  let NewTabUtils = Cu.import("resource://gre/modules/NewTabUtils.jsm", {}).NewTabUtils;
  let Promise = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;

  // resolves promise when directory links are downloaded and written to disk
  function watchLinksChangeOnce() {
    let deferred = Promise.defer();
    let observer = {
      onManyLinksChanged: () => {
        DirectoryLinksProvider.removeObserver(observer);
        NewTabUtils.links.populateCache(() => {
          NewTabUtils.allPages.update();
          deferred.resolve();
        }, true);
      }
    };
    observer.onDownloadFail = observer.onManyLinksChanged;
    DirectoryLinksProvider.addObserver(observer);
    return deferred.promise;
  }

  let gOrigDirectorySource = Services.prefs.getCharPref(PREF_NEWTAB_DIRECTORYSOURCE);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PREF_PRELOAD);
    Services.prefs.setCharPref(PREF_NEWTAB_DIRECTORYSOURCE, gOrigDirectorySource);
    return watchLinksChangeOnce();
  });

  Services.prefs.setBoolPref(PREF_PRELOAD, false);
  // set directory source to dummy/empty links
  Services.prefs.setCharPref(PREF_NEWTAB_DIRECTORYSOURCE, 'data:application/json,{"test":1}');

  // run tests when directory source change completes
  yield watchLinksChangeOnce();

  // Perform a click in the top left of content to ensure the mouse isn't
  // hovering over any of the tiles
  let target = gBrowser.selectedBrowser;
  let rect = target.getBoundingClientRect();
  let left = rect.left + 1;
  let top = rect.top + 1;

  let utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
  utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
  utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);

  // Add a reflow observer and open a new tab.
  docShell.addWeakReflowObserver(observer);
  BrowserOpenTab();

  // Wait until the tabopen animation has finished.
  yield waitForTransitionEnd();

  // Remove reflow observer and clean up.
  docShell.removeWeakReflowObserver(observer);
  gBrowser.removeCurrentTab();
});

var observer = {
  reflow: function(start, end) {
    // Gather information about the current code path.
    let path = (new Error().stack).split("\n").slice(1).map(line => {
      return line.replace(/:\d+:\d+$/, "");
    }).join("|");
    let pathWithLineNumbers = (new Error().stack).split("\n").slice(1).join("|");

    // Stack trace is empty. Reflow was triggered by native code.
    if (path === "") {
      return;
    }

    // Check if this is an expected reflow.
    for (let stack of EXPECTED_REFLOWS) {
      if (path.startsWith(stack)) {
        ok(true, "expected uninterruptible reflow '" + stack + "'");
        return;
      }
    }

    ok(false, "unexpected uninterruptible reflow '" + pathWithLineNumbers + "'");
  },

  reflowInterruptible: function(start, end) {
    // We're not interested in interruptible reflows.
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIReflowObserver,
                                         Ci.nsISupportsWeakReference])
};

function waitForTransitionEnd() {
  return new Promise(resolve => {
    let tab = gBrowser.selectedTab;
    tab.addEventListener("transitionend", function onEnd(event) {
      if (event.propertyName === "max-width") {
        tab.removeEventListener("transitionend", onEnd);
        resolve();
      }
    });
  });
}
