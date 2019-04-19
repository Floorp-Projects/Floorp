/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gBrowser;
var gProgressListener;
var gDebugger;

const NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID = "@mozilla.org/layout-debug/layout-debuggingtools;1";
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

function nsLDBBrowserContentListener()
{
  this.init();
}

nsLDBBrowserContentListener.prototype = {

  init : function()
    {
      this.mStatusText = document.getElementById("status-text");
      this.mURLBar = document.getElementById("urlbar");
      this.mForwardButton = document.getElementById("forward-button");
      this.mBackButton = document.getElementById("back-button");
      this.mStopButton = document.getElementById("stop-button");
    },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener,
                                          Ci.nsISupportsWeakReference]),

  // nsIWebProgressListener implementation
  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
    {
      if (!(aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) ||
          aWebProgress != gBrowser.webProgress)
        return;

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

  onProgressChange : function(aWebProgress, aRequest,
                              aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress)
    {
    },

  onLocationChange : function(aWebProgress, aRequest, aLocation, aFlags)
    {
      this.mURLBar.value = aLocation.spec;
      this.setButtonEnabled(this.mForwardButton, gBrowser.canGoForward);
      this.setButtonEnabled(this.mBackButton, gBrowser.canGoBack);
    },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
    {
      this.mStatusText.value = aMessage;
    },

  onSecurityChange : function(aWebProgress, aRequest, aState)
    {
    },

  onContentBlockingEvent : function(aWebProgress, aRequest, aEvent)
    {
    },

  // non-interface methods
  setButtonEnabled : function(aButtonElement, aEnabled)
    {
      if (aEnabled)
        aButtonElement.removeAttribute("disabled");
      else
        aButtonElement.setAttribute("disabled", "true");
    },

  mStatusText : null,
  mURLBar : null,
  mForwardButton : null,
  mBackButton : null,
  mStopButton : null,

  mLoading : false

}

function OnLDBLoad()
{
  gBrowser = document.getElementById("browser");

  gProgressListener = new nsLDBBrowserContentListener();
  gBrowser.addProgressListener(gProgressListener);

  gDebugger = Cc[NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID].
                  createInstance(Ci.nsILayoutDebuggingTools);

  if (window.arguments && window.arguments[0]) {
    gBrowser.loadURI(window.arguments[0], {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  } else {
    gBrowser.loadURI("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
    });
  }

  gDebugger.init(gBrowser.contentWindow);

  checkPersistentMenus();
}

function checkPersistentMenu(item)
{
  var menuitem = document.getElementById("menu_" + item);
  menuitem.setAttribute("checked", gDebugger[item]);
}

function checkPersistentMenus()
{
  // Restore the toggles that are stored in prefs.
  checkPersistentMenu("paintFlashing");
  checkPersistentMenu("paintDumping");
  checkPersistentMenu("invalidateDumping");
  checkPersistentMenu("eventDumping");
  checkPersistentMenu("motionEventDumping");
  checkPersistentMenu("crossingEventDumping");
  checkPersistentMenu("reflowCounts");
}


function OnLDBUnload()
{
  gBrowser.removeProgressListener(gProgressListener);
}

function toggle(menuitem)
{
  // trim the initial "menu_"
  var feature = menuitem.id.substring(5);
  gDebugger[feature] = menuitem.getAttribute("checked") == "true";
}

function openFile()
{
  var fp = Cc["@mozilla.org/filepicker;1"]
        .createInstance(Ci.nsIFilePicker);
  fp.init(window, "Select a File", Ci.nsIFilePicker.modeOpen);
  fp.appendFilters(Ci.nsIFilePicker.filterHTML | Ci.nsIFilePicker.filterAll);
  fp.open(rv => {
    if (rv == Ci.nsIFilePicker.returnOK && fp.fileURL.spec &&
        fp.fileURL.spec.length > 0) {
      gBrowser.loadURI(fp.fileURL.spec, {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    }
  });
}
