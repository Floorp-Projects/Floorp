/**
 * In this test, we call the save panel with CTRL+S. When shown, we load a
 * webpage that is going to open malicious popups. These popups should be
 * disallowed.
 */

var gLoaded = false;

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockObjects.js",
                 this);

function MockFilePicker() { }

MockFilePicker.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFilePicker]),

  init: function() { },

  appendFilters: function(val) { },
  appendFilter: function(val) { },

  // constants
  modeOpen: 0,
  modeSave: 1,
  modeGetFolder: 2,
  modeOpenMultiple: 3,
  returnOK: 0,
  returnCancel: 1,
  returnReplace: 2,
  filterAll: 1,
  filterHTML: 2,
  filterText: 4,
  filterImages: 8,
  filterXML: 16,
  filterXUL: 32,
  filterApps: 64,
  filterAllowURLs: 128,
  filterAudio: 256,
  filterVideo: 512,

  // properties
  defaultExtension: "",
  defaultString: "",
  get displayDirectory() { return null; },
  set displayDirectory(val) { },
  file: null,
  get files() { return null; },
  get fileURL() { return null; },
  filterIndex: 0,

  show: function() {
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

    return this.returnCancel;
  },
};

function test() {
  waitForExplicitFinish();

  var mockFilePickerRegisterer =
    new MockObjectRegisterer("@mozilla.org/filepicker;1", MockFilePicker);
  mockFilePickerRegisterer.register();

  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
            .getService(Components.interfaces.nsIPrefBranch);
  var gDisableLoadPref = prefs.getBoolPref("dom.disable_open_during_load");
  prefs.setBoolPref("dom.disable_open_during_load", true);

  gBrowser.addEventListener("DOMPopupBlocked", function() {
    gBrowser.removeEventListener("DOMPopupBlocked", arguments.callee, true);
    ok(true, "The popup has been blocked");
    prefs.setBoolPref("dom.disable_open_during_load", gDisableLoadPref);

    mockFilePickerRegisterer.unregister();

    finish();
  }, true)

  if (navigator.platform.indexOf("Mac") == 0) {
    // MacOS use metaKey instead of ctrlKey.
    EventUtils.synthesizeKey("s", { metaKey: true, });
  } else {
    EventUtils.synthesizeKey("s", { ctrlKey: true, });
  }
}
