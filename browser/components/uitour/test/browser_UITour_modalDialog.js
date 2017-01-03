"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;
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
        const interfaces = [Ci.nsIObserver,
                            Ci.nsISupports, Ci.nsISupportsWeakReference];

        if (!interfaces.some( function(v) { return iid.equals(v) } ))
            throw SpecialPowers.Components.results.NS_ERROR_NO_INTERFACE;
        return this;
    },

    observe(subject, topic, data) {
        var doc = getDialogDoc();
        if (doc)
            handleDialog(doc);
        else
            startCallbackTimer(); // try again in a bit
    }
});

function getDialogDoc() {
  // Find the <browser> which contains notifyWindow, by looking
  // through all the open windows and all the <browsers> in each.
  var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
           getService(Ci.nsIWindowMediator);
  // var enumerator = wm.getEnumerator("navigator:browser");
  var enumerator = wm.getXULWindowEnumerator(null);

  while (enumerator.hasMoreElements()) {
    var win = enumerator.getNext();
    var windowDocShell = win.QueryInterface(Ci.nsIXULWindow).docShell;

    var containedDocShells = windowDocShell.getDocShellEnumerator(
                                      Ci.nsIDocShellTreeItem.typeChrome,
                                      Ci.nsIDocShell.ENUMERATE_FORWARDS);
    while (containedDocShells.hasMoreElements()) {
        // Get the corresponding document for this docshell
        var childDocShell = containedDocShells.getNext();
        // We don't want it if it's not done loading.
        if (childDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE)
          continue;
        var childDoc = childDocShell.QueryInterface(Ci.nsIDocShell)
                                    .contentViewer
                                    .DOMDocument;

        // ok(true, "Got window: " + childDoc.location.href);
        if (childDoc.location.href == "chrome://global/content/commonDialog.xul")
          return childDoc;
    }
  }

  return null;
}

function test() {
  UITourTest();
}


var tests = [
  taskify(function* test_modal_dialog_while_opening_tooltip() {
    let panelShown;
    let popup;

    handleDialog = (doc) => {
      popup = document.getElementById("UITourTooltip");
      gContentAPI.showInfo("appMenu", "test title", "test text");
      doc.defaultView.setTimeout(function() {
        is(popup.state, "closed", "Popup shouldn't be shown while dialog is up");
        panelShown = promisePanelElementShown(window, popup);
        let dialog = doc.getElementById("commonDialog");
        dialog.acceptDialog();
      }, 1000);
    };
    startCallbackTimer();
    executeSoon(() => alert("test"));
    yield waitForConditionPromise(() => panelShown, "Timed out waiting for panel promise to be assigned", 100);
    yield panelShown;

    yield hideInfoPromise();
  })
];
