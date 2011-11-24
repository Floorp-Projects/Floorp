/**
 * In this test, we call the save panel with CTRL+S. When shown, we load a
 * webpage that is going to open malicious popups. These popups should be
 * disallowed.
 */

var gLoaded = false;

var MockFilePicker = SpecialPowers.MockFilePicker;
function onShowCallback() {
    gBrowser.selectedTab.linkedBrowser.addEventListener("load", function () {
      gBrowser.selectedTab.linkedBrowser.removeEventListener("load", arguments.callee, true);
      executeSoon(function() {
        gLoaded = true;
      });
    }, true);

    gBrowser.selectedTab.linkedBrowser.loadURI("data:text/html,<!DOCTYPE html><html><body onload='window.open(\"about:blank\", \"\", \"width=200,height=200\");'></body></html>");

    let curThread = Components.classes["@mozilla.org/thread-manager;1"]
                              .getService().currentThread;
    while (!gLoaded) {
      curThread.processNextEvent(true);
    }

    MockFilePicker.returnValue = MockFilePicker.returnCancel;
};

function test() {
  waitForExplicitFinish();

  MockFilePicker.reset();
  MockFilePicker.showCallback = onShowCallback;

  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
            .getService(Components.interfaces.nsIPrefBranch);
  var gDisableLoadPref = prefs.getBoolPref("dom.disable_open_during_load");
  prefs.setBoolPref("dom.disable_open_during_load", true);

  gBrowser.addEventListener("DOMPopupBlocked", function() {
    gBrowser.removeEventListener("DOMPopupBlocked", arguments.callee, true);
    ok(true, "The popup has been blocked");
    prefs.setBoolPref("dom.disable_open_during_load", gDisableLoadPref);

    MockFilePicker.reset();

    finish();
  }, true)

  if (navigator.platform.indexOf("Mac") == 0) {
    // MacOS use metaKey instead of ctrlKey.
    EventUtils.synthesizeKey("s", { metaKey: true, });
  } else {
    EventUtils.synthesizeKey("s", { ctrlKey: true, });
  }
}
