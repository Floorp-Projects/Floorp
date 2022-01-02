"use strict";

/* eslint-disable mozilla/use-chromeutils-generateqi */

var gTestTab;
var gContentAPI;
var handleDialog;

// Modified from toolkit/components/passwordmgr/test/prompt_common.js
var didDialog;

var timer; // keep in outer scope so it's not GC'd before firing
function startCallbackTimer() {
  didDialog = false;

  // Delay before the callback twiddles the prompt.
  const dialogDelay = 10;

  // Use a timer to invoke a callback to twiddle the authentication dialog
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(observer, dialogDelay, Ci.nsITimer.TYPE_ONE_SHOT);
}

var observer = SpecialPowers.wrapCallbackObject({
  QueryInterface(iid) {
    const interfaces = [
      Ci.nsIObserver,
      Ci.nsISupports,
      Ci.nsISupportsWeakReference,
    ];

    if (
      !interfaces.some(function(v) {
        return iid.equals(v);
      })
    ) {
      throw SpecialPowers.Components.results.NS_ERROR_NO_INTERFACE;
    }
    return this;
  },

  observe(subject, topic, data) {
    var doc = getDialogDoc();
    if (doc) {
      handleDialog(doc);
    } else {
      startCallbackTimer();
    } // try again in a bit
  },
});

function getDialogDoc() {
  // Find the <browser> which contains notifyWindow, by looking
  // through all the open windows and all the <browsers> in each.

  // var enumerator = wm.getEnumerator("navigator:browser");
  for (let { docShell } of Services.wm.getEnumerator(null)) {
    var containedDocShells = docShell.getAllDocShellsInSubtree(
      docShell.typeChrome,
      docShell.ENUMERATE_FORWARDS
    );
    for (let childDocShell of containedDocShells) {
      // Get the corresponding document for this docshell
      // We don't want it if it's not done loading.
      if (childDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE) {
        continue;
      }
      var childDoc = childDocShell.contentViewer.DOMDocument;

      // ok(true, "Got window: " + childDoc.location.href);
      if (
        childDoc.location.href == "chrome://global/content/commonDialog.xhtml"
      ) {
        return childDoc;
      }
    }
  }

  return null;
}

function test() {
  UITourTest();
}

var tests = [
  taskify(async function test_modal_dialog_while_opening_tooltip() {
    let panelShown;
    let popup;

    handleDialog = doc => {
      popup = document.getElementById("UITourTooltip");
      gContentAPI.showInfo("appMenu", "test title", "test text");
      doc.defaultView.setTimeout(function() {
        is(
          popup.state,
          "closed",
          "Popup shouldn't be shown while dialog is up"
        );
        panelShown = promisePanelElementShown(window, popup);
        let dialog = doc.getElementById("commonDialog");
        dialog.acceptDialog();
      }, 1000);
    };
    startCallbackTimer();
    executeSoon(() => alert("test"));
    await waitForConditionPromise(
      () => panelShown,
      "Timed out waiting for panel promise to be assigned",
      100
    );
    await panelShown;

    await hideInfoPromise();
  }),
];
