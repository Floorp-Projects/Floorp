/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gBrowser;
var gProgressListener;
var gDebugger;

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const HELPER_URL = "chrome://layoutdebug/content/layoutdebug-helper.js";

const FEATURES = {
  paintFlashing: "nglayout.debug.paint_flashing",
  paintDumping: "nglayout.debug.paint_dumping",
  invalidateDumping: "nglayout.debug.invalidate_dumping",
  eventDumping: "nglayout.debug.event_dumping",
  motionEventDumping: "nglayout.debug.motion_event_dumping",
  crossingEventDumping: "nglayout.debug.crossing_event_dumping",
  reflowCounts: "layout.reflow.showframecounts",
};

const COMMANDS = [
  "dumpWebShells",
  "dumpContent",
  "dumpFrames",
  "dumpViews",
  "dumpStyleSheets",
  "dumpMatchedRules",
  "dumpComputedStyles",
  "dumpReflowStats",
];

class Debugger {
  constructor() {
    this._flags = new Map();
    this._visualDebugging = false;
    this._visualEventDebugging = false;

    for (let [name, pref] of Object.entries(FEATURES)) {
      this._flags.set(name, !!Preferences.get(pref, false));
    }
    gBrowser.messageManager.loadFrameScript(HELPER_URL, false);
  }

  get visualDebugging() {
    return this._visualDebugging;
  }

  set visualDebugging(v) {
    v = !!v;
    this._visualDebugging = v;
    this._sendMessage("setVisualDebugging", v);
  }

  get visualEventDebugging() {
    return this._visualEventDebugging;
  }

  set visualEventDebugging(v) {
    v = !!v;
    this._visualEventDebugging = v;
    this._sendMessage("setVisualEventDebugging", v);
  }

  _sendMessage(name, arg) {
    gBrowser.messageManager.sendAsyncMessage("LayoutDebug:Call", { name, arg });
  }
}

for (let [name, pref] of Object.entries(FEATURES)) {
  Object.defineProperty(Debugger.prototype, name, {
    get: function() {
      return this._flags.get(name);
    },
    set: function(v) {
      v = !!v;
      Preferences.set(pref, v);
      this._flags.set(name, v);
      // XXX PresShell should watch for this pref change itself.
      if (name == "reflowCounts") {
        this._sendMessage("setReflowCounts", v);
      }
      this._sendMessage("forceRefresh");
    },
  });
}

for (let name of COMMANDS) {
  Debugger.prototype[name] = function() {
    this._sendMessage(name);
  };
}

function nsLDBBrowserContentListener() {
  this.init();
}

nsLDBBrowserContentListener.prototype = {
  init: function() {
    this.mStatusText = document.getElementById("status-text");
    this.mURLBar = document.getElementById("urlbar");
    this.mForwardButton = document.getElementById("forward-button");
    this.mBackButton = document.getElementById("back-button");
    this.mStopButton = document.getElementById("stop-button");
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
  ]),

  // nsIWebProgressListener implementation
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (
      !(aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) ||
      aWebProgress != gBrowser.webProgress
    ) {
      return;
    }

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
      this.setButtonEnabled(this.mStopButton, true);
      this.setButtonEnabled(this.mForwardButton, gBrowser.canGoForward);
      this.setButtonEnabled(this.mBackButton, gBrowser.canGoBack);
      this.mStatusText.value = "loading...";
      this.mLoading = true;
    } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      this.setButtonEnabled(this.mStopButton, false);
      this.mStatusText.value = this.mURLBar.value + " loaded";
      this.mLoading = false;
    }
  },

  onProgressChange: function(
    aWebProgress,
    aRequest,
    aCurSelfProgress,
    aMaxSelfProgress,
    aCurTotalProgress,
    aMaxTotalProgress
  ) {},

  onLocationChange: function(aWebProgress, aRequest, aLocation, aFlags) {
    this.mURLBar.value = aLocation.spec;
    this.setButtonEnabled(this.mForwardButton, gBrowser.canGoForward);
    this.setButtonEnabled(this.mBackButton, gBrowser.canGoBack);
  },

  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage) {
    this.mStatusText.value = aMessage;
  },

  onSecurityChange: function(aWebProgress, aRequest, aState) {},

  onContentBlockingEvent: function(aWebProgress, aRequest, aEvent) {},

  // non-interface methods
  setButtonEnabled: function(aButtonElement, aEnabled) {
    if (aEnabled) {
      aButtonElement.removeAttribute("disabled");
    } else {
      aButtonElement.setAttribute("disabled", "true");
    }
  },

  mStatusText: null,
  mURLBar: null,
  mForwardButton: null,
  mBackButton: null,
  mStopButton: null,

  mLoading: false,
};

function OnLDBLoad() {
  gBrowser = document.getElementById("browser");

  gProgressListener = new nsLDBBrowserContentListener();
  gBrowser.addProgressListener(gProgressListener);

  gDebugger = new Debugger();

  if (window.arguments && window.arguments[0]) {
    gBrowser.loadURI(window.arguments[0], {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  } else {
    gBrowser.loadURI("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    });
  }

  checkPersistentMenus();
}

function checkPersistentMenu(item) {
  var menuitem = document.getElementById("menu_" + item);
  menuitem.setAttribute("checked", gDebugger[item]);
}

function checkPersistentMenus() {
  // Restore the toggles that are stored in prefs.
  checkPersistentMenu("paintFlashing");
  checkPersistentMenu("paintDumping");
  checkPersistentMenu("invalidateDumping");
  checkPersistentMenu("eventDumping");
  checkPersistentMenu("motionEventDumping");
  checkPersistentMenu("crossingEventDumping");
  checkPersistentMenu("reflowCounts");
}

function OnLDBUnload() {
  gBrowser.removeProgressListener(gProgressListener);
}

function toggle(menuitem) {
  // trim the initial "menu_"
  var feature = menuitem.id.substring(5);
  gDebugger[feature] = menuitem.getAttribute("checked") == "true";
}

function openFile() {
  var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  fp.init(window, "Select a File", Ci.nsIFilePicker.modeOpen);
  fp.appendFilters(Ci.nsIFilePicker.filterHTML | Ci.nsIFilePicker.filterAll);
  fp.open(rv => {
    if (
      rv == Ci.nsIFilePicker.returnOK &&
      fp.fileURL.spec &&
      fp.fileURL.spec.length > 0
    ) {
      gBrowser.loadURI(fp.fileURL.spec, {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    }
  });
}
