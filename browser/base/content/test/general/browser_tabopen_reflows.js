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

  // unpreloaded newtab pages explicitly waits for reflows for sizing
  "gPage.onPageFirstVisible/checkSizing/<@chrome://browser/content/newtab/newTab.js|",

  // accessing element.scrollPosition in _fillTrailingGap() flushes layout
  "get_scrollPosition@chrome://global/content/bindings/scrollbox.xml|" +
    "_fillTrailingGap@chrome://browser/content/tabbrowser.xml|" +
    "_handleNewTab@chrome://browser/content/tabbrowser.xml|" +
    "onxbltransitionend@chrome://browser/content/tabbrowser.xml|",

  // The TabView iframe causes reflows in the parent document.
  "iQClass_height@chrome://browser/content/tabview.js|" +
    "GroupItem_getContentBounds@chrome://browser/content/tabview.js|" +
    "GroupItem_shouldStack@chrome://browser/content/tabview.js|" +
    "GroupItem_arrange@chrome://browser/content/tabview.js|" +
    "GroupItem_add@chrome://browser/content/tabview.js|" +
    "GroupItems_newTab@chrome://browser/content/tabview.js|" +
    "TabItem__reconnect@chrome://browser/content/tabview.js|" +
    "TabItem@chrome://browser/content/tabview.js|" +
    "TabItems_link@chrome://browser/content/tabview.js|" +
    "TabItems_init/this._eventListeners.open@chrome://browser/content/tabview.js|",

  // SessionStore.getWindowDimensions()
  "ssi_getWindowDimension@resource:///modules/sessionstore/SessionStore.jsm|" +
    "ssi_updateWindowFeatures/<@resource:///modules/sessionstore/SessionStore.jsm|" +
    "ssi_updateWindowFeatures@resource:///modules/sessionstore/SessionStore.jsm|" +
    "ssi_collectWindowData@resource:///modules/sessionstore/SessionStore.jsm|",

  // tabPreviews.capture()
  "tabPreviews_capture@chrome://browser/content/browser.js|" +
    "tabPreviews_handleEvent/<@chrome://browser/content/browser.js|",

  // tabPreviews.capture()
  "tabPreviews_capture@chrome://browser/content/browser.js|" +
    "@chrome://browser/content/browser.js|"
];

const PREF_PRELOAD = "browser.newtab.preload";
const PREF_NEWTAB_DIRECTORYSOURCE = "browser.newtabpage.directory.source";

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when opening new tabs.
 */
function test() {
  waitForExplicitFinish();
  let DirectoryLinksProvider = Cu.import("resource://gre/modules/DirectoryLinksProvider.jsm", {}).DirectoryLinksProvider;
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
  };

  let gOrigDirectorySource = Services.prefs.getCharPref(PREF_NEWTAB_DIRECTORYSOURCE);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PREF_PRELOAD);
    Services.prefs.setCharPref(PREF_NEWTAB_DIRECTORYSOURCE, gOrigDirectorySource);
    return watchLinksChangeOnce();
  });

  // run tests when directory source change completes
  watchLinksChangeOnce().then(() => {
    // Add a reflow observer and open a new tab.
    docShell.addWeakReflowObserver(observer);
    BrowserOpenTab();

    // Wait until the tabopen animation has finished.
    waitForTransitionEnd(function () {
      // Remove reflow observer and clean up.
      docShell.removeWeakReflowObserver(observer);
      gBrowser.removeCurrentTab();
      finish();
    });
  });

  Services.prefs.setBoolPref(PREF_PRELOAD, false);
  // set directory source to dummy/empty links
  Services.prefs.setCharPref(PREF_NEWTAB_DIRECTORYSOURCE, 'data:application/json,{"test":1}');
}

let observer = {
  reflow: function (start, end) {
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

  reflowInterruptible: function (start, end) {
    // We're not interested in interruptible reflows.
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIReflowObserver,
                                         Ci.nsISupportsWeakReference])
};

function waitForTransitionEnd(callback) {
  let tab = gBrowser.selectedTab;
  tab.addEventListener("transitionend", function onEnd(event) {
    if (event.propertyName === "max-width") {
      tab.removeEventListener("transitionend", onEnd);
      executeSoon(callback);
    }
  });
}
