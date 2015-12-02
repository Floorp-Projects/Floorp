/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPECTED_REFLOWS = [
  // handleEvent flushes layout to get the tabstrip width after a resize.
  "handleEvent@chrome://browser/content/tabbrowser.xml|",

  // Loading a tab causes a reflow.
  "loadTabs@chrome://browser/content/tabbrowser.xml|" +
    "loadOneOrMoreURIs@chrome://browser/content/browser.js|" +
    "gBrowserInit._delayedStartup@chrome://browser/content/browser.js|",

  // Selecting the address bar causes a reflow.
  "select@chrome://global/content/bindings/textbox.xml|" +
    "focusAndSelectUrlBar@chrome://browser/content/browser.js|" +
    "gBrowserInit._delayedStartup@chrome://browser/content/browser.js|",

  // Focusing the content area causes a reflow.
  "gBrowserInit._delayedStartup@chrome://browser/content/browser.js|",

  // Sometimes sessionstore collects data during this test, which causes a sync reflow
  // (https://bugzilla.mozilla.org/show_bug.cgi?id=892154 will fix this)
  "ssi_getWindowDimension@resource:///modules/sessionstore/SessionStore.jsm",
];

if (Services.appinfo.OS == "WINNT" || Services.appinfo.OS == "Darwin") {
  // TabsInTitlebar._update causes a reflow on OS X and Windows trying to do calculations
  // since layout info is already dirty. This doesn't seem to happen before
  // MozAfterPaint on Linux.
  EXPECTED_REFLOWS.push("TabsInTitlebar._update/rect@chrome://browser/content/browser-tabsintitlebar.js|" +
                          "TabsInTitlebar._update@chrome://browser/content/browser-tabsintitlebar.js|" +
                          "updateAppearance@chrome://browser/content/browser-tabsintitlebar.js|" +
                          "handleEvent@chrome://browser/content/tabbrowser.xml|");
}

if (Services.appinfo.OS == "Darwin") {
  // _onOverflow causes a reflow getting widths.
  EXPECTED_REFLOWS.push("OverflowableToolbar.prototype._onOverflow@resource:///modules/CustomizableUI.jsm|" +
                        "OverflowableToolbar.prototype.init@resource:///modules/CustomizableUI.jsm|" +
                        "OverflowableToolbar.prototype.observe@resource:///modules/CustomizableUI.jsm|" +
                        "gBrowserInit._delayedStartup@chrome://browser/content/browser.js|");
  // Same as above since in packaged builds there are no function names and the resource URI includes "app"
  EXPECTED_REFLOWS.push("@resource://app/modules/CustomizableUI.jsm|" +
                          "@resource://app/modules/CustomizableUI.jsm|" +
                          "@resource://app/modules/CustomizableUI.jsm|" +
                          "gBrowserInit._delayedStartup@chrome://browser/content/browser.js|");
}

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when opening new windows.
 */
function test() {
  waitForExplicitFinish();

  // Add a reflow observer and open a new window
  let win = OpenBrowserWindow();
  let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShell);
  docShell.addWeakReflowObserver(observer);

  // Wait until the mozafterpaint event occurs.
  waitForMozAfterPaint(win, function paintListener() {
    // Remove reflow observer and clean up.
    docShell.removeWeakReflowObserver(observer);
    win.close();

    finish();
  });
}

var observer = {
  reflow: function (start, end) {
    // Gather information about the current code path.
    let stack = new Error().stack;
    let path = stack.split("\n").slice(1).map(line => {
      return line.replace(/:\d+:\d+$/, "");
    }).join("|");
    let pathWithLineNumbers = (new Error().stack).split("\n").slice(1).join("|");

    // Stack trace is empty. Reflow was triggered by native code.
    if (path === "") {
      return;
    }

    // Check if this is an expected reflow.
    for (let expectedStack of EXPECTED_REFLOWS) {
      if (path.startsWith(expectedStack) ||
          // Accept an empty function name for gBrowserInit._delayedStartup or TabsInTitlebar._update to workaround bug 906578.
          path.startsWith(expectedStack.replace(/(^|\|)(gBrowserInit\._delayedStartup|TabsInTitlebar\._update)@/, "$1@"))) {
        ok(true, "expected uninterruptible reflow '" + expectedStack + "'");
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

function waitForMozAfterPaint(win, callback) {
  win.addEventListener("MozAfterPaint", function onEnd(event) {
    if (event.target != win)
      return;
    win.removeEventListener("MozAfterPaint", onEnd);
    executeSoon(callback);
  });
}
