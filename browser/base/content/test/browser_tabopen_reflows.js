/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyGetter(this, "docShell", () => {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsIDocShell);
});

const EXPECTED_REFLOWS = [
  // b.stop() call in tabbrowser.addTab()
  "stop@chrome://global/content/bindings/browser.xml|" +
    "addTab@chrome://browser/content/tabbrowser.xml|",

  // tabbrowser.adjustTabstrip() call after tabopen animation has finished
  "adjustTabstrip@chrome://browser/content/tabbrowser.xml|" +
    "_handleNewTab@chrome://browser/content/tabbrowser.xml|" +
    "onxbltransitionend@chrome://browser/content/tabbrowser.xml|",

  // switching focus in updateCurrentBrowser() causes reflows
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
    "onxbltransitionend@chrome://browser/content/tabbrowser.xml|"
];

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when opening new tabs.
 */
function test() {
  waitForExplicitFinish();

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
}

let observer = {
  reflow: function (start, end) {
    // Gather information about the current code path.
    let path = (new Error().stack).split("\n").slice(1).map(line => {
      return line.replace(/:\d+$/, "");
    }).join("|");

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

    ok(false, "unexpected uninterruptible reflow '" + path + "'");
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
