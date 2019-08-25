/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gArgs;
var gBrowser;
var gURLBar;
var gDebugger;
var gMultiProcessBrowser = window.docShell.QueryInterface(Ci.nsILoadContext)
  .useRemoteTabs;
var gFissionBrowser = window.docShell.QueryInterface(Ci.nsILoadContext)
  .useRemoteSubframes;

const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);
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
    this._attached = false;

    for (let [name, pref] of Object.entries(FEATURES)) {
      this._flags.set(name, !!Preferences.get(pref, false));
    }

    this.attachBrowser();
  }

  detachBrowser() {
    if (!this._attached) {
      return;
    }
    gBrowser.removeProgressListener(this._progressListener);
    this._progressListener = null;
    this._attached = false;
  }

  attachBrowser() {
    if (this._attached) {
      throw "already attached";
    }
    this._progressListener = new nsLDBBrowserContentListener();
    gBrowser.addProgressListener(this._progressListener);
    gBrowser.messageManager.loadFrameScript(HELPER_URL, false);
    this._attached = true;
  }

  dumpProcessIDs() {
    let parentPid = Services.appinfo.processID;
    let [contentPid, ...framePids] = E10SUtils.getBrowserPids(
      gBrowser,
      gFissionBrowser
    );

    dump(`Parent pid: ${parentPid}\n`);
    dump(`Content pid: ${contentPid || "-"}\n`);
    if (gFissionBrowser) {
      dump(`Subframe pids: ${framePids.length ? framePids.join(", ") : "-"}\n`);
    }
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
    if (!(aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK)) {
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
      this.mStatusText.value = gURLBar.value + " loaded";
      this.mLoading = false;
      if (gArgs.autoclose && gBrowser.currentURI.spec != "about:blank") {
        // We check for about:blank just to avoid one or two STATE_STOP
        // notifications that occur before the loadURI() call completes.
        // This does mean that --autoclose doesn't work when the URL on
        // the command line is about:blank (or not specified), but that's
        // not a big deal.
        setTimeout(
          () => Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit),
          gArgs.delay * 1000
        );
      }
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
    gURLBar.value = aLocation.spec;
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
  mForwardButton: null,
  mBackButton: null,
  mStopButton: null,

  mLoading: false,
};

function parseArguments() {
  let args = {
    url: null,
    autoclose: false,
    delay: 0,
  };
  if (window.arguments) {
    args.url = window.arguments[0];
    for (let i = 1; i < window.arguments.length; ++i) {
      if (/^autoclose=(.*)$/.test(window.arguments[i])) {
        args.autoclose = true;
        args.delay = +RegExp.$1;
      } else {
        throw `Unknown option ${window.arguments[i]}`;
      }
    }
  }
  return args;
}

function OnLDBLoad() {
  gBrowser = document.getElementById("browser");
  gURLBar = document.getElementById("urlbar");

  gDebugger = new Debugger();

  checkPersistentMenus();

  // Pretend slightly to be like a normal browser, so that SessionStore.jsm
  // doesn't get too confused.  The effect is that we'll never switch process
  // type when navigating, and for layout debugging purposes we don't bother
  // about getting that right.
  gBrowser.getTabForBrowser = function() {
    return null;
  };

  gArgs = parseArguments();
  if (gArgs.url) {
    loadURI(gArgs.url);
  }
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
  gDebugger.detachBrowser();
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
      loadURI(fp.fileURL.spec);
    }
  });
}

// A simplified version of the function with the same name in tabbrowser.js.
function updateBrowserRemotenessByURL(aURL) {
  let remoteType = E10SUtils.getRemoteTypeForURI(
    aURL,
    gMultiProcessBrowser,
    gFissionBrowser,
    gBrowser.remoteType,
    gBrowser.currentURI
  );
  if (gBrowser.remoteType != remoteType) {
    gDebugger.detachBrowser();
    if (remoteType == E10SUtils.NOT_REMOTE) {
      gBrowser.removeAttribute("remote");
      gBrowser.removeAttribute("remoteType");
    } else {
      gBrowser.setAttribute("remote", "true");
      gBrowser.setAttribute("remoteType", remoteType);
    }
    if (
      !Services.prefs.getBoolPref(
        "fission.rebuild_frameloaders_on_remoteness_change",
        false
      )
    ) {
      gBrowser.replaceWith(gBrowser);
    } else {
      gBrowser.changeRemoteness({ remoteType });
      gBrowser.construct();
    }
    gDebugger.attachBrowser();
  }
}

function loadURI(aURL) {
  // We don't bother trying to handle navigations within the browser to new URLs
  // that should be loaded in a different process.
  updateBrowserRemotenessByURL(aURL);
  gBrowser.loadURI(aURL, {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
}

function focusURLBar() {
  gURLBar.focus();
  gURLBar.select();
}

function go() {
  loadURI(gURLBar.value);
  gBrowser.focus();
}
