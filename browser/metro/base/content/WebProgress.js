/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Progress heartbeat timer duration (ms)
const kHeartbeatDuration = 1000;
// Start and end progress screen css margins as percentages
const kProgressMarginStart = 30;
const kProgressMarginEnd = 70;

const WebProgress = {
  _progressActive: false,

  init: function init() {
    messageManager.addMessageListener("Content:StateChange", this);
    messageManager.addMessageListener("Content:LocationChange", this);
    messageManager.addMessageListener("Content:SecurityChange", this);
    Elements.progress.addEventListener("transitionend", this._progressTransEnd, true);
    return this;
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    let tab = Browser.getTabForBrowser(aMessage.target);

    switch (aMessage.name) {
      case "Content:StateChange": {
        if (json.stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
          if (json.stateFlags & Ci.nsIWebProgressListener.STATE_START)
            this._windowStart(json, tab);
          else if (json.stateFlags & Ci.nsIWebProgressListener.STATE_STOP)
            this._windowStop(json, tab);
        }

        if (json.stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
          if (json.stateFlags & Ci.nsIWebProgressListener.STATE_START)
            this._networkStart(json, tab);
          else if (json.stateFlags & Ci.nsIWebProgressListener.STATE_STOP)
            this._networkStop(json, tab);
        }

        this._progressStep();
        break;
      }

      case "Content:LocationChange": {
        this._locationChange(json, tab);
        this._progressStep();
        break;
      }

      case "Content:SecurityChange": {
        this._securityChange(json, tab);
        this._progressStep();
        break;
      }
    }
  },

  _securityChange: function _securityChange(aJson, aTab) {
    // Don't need to do anything if the data we use to update the UI hasn't changed
    if (aTab.state == aJson.state && !aTab.hostChanged)
      return;

    aTab.hostChanged = false;
    aTab.state = aJson.state;
  },

  _locationChange: function _locationChange(aJson, aTab) {
    let spec = aJson.location;
    let location = spec.split("#")[0]; // Ignore fragment identifier changes.

    if (aTab == Browser.selectedTab)
      BrowserUI.updateURI();

    let locationHasChanged = (location != aTab.browser.lastLocation);
    if (locationHasChanged) {
      Browser.getNotificationBox(aTab.browser).removeTransientNotifications();
      aTab.resetZoomLevel();
      aTab.hostChanged = true;
      aTab.browser.lastLocation = location;
      aTab.browser.userTypedValue = "";
      aTab.browser.appIcon = { href: null, size:-1 };

#ifdef MOZ_CRASHREPORTER
      if (CrashReporter.enabled)
        CrashReporter.annotateCrashReport("URL", spec);
#endif
      this._waitForLoad(aTab);
    }

    let event = document.createEvent("UIEvents");
    event.initUIEvent("URLChanged", true, false, window, locationHasChanged);
    aTab.browser.dispatchEvent(event);
  },

  _waitForLoad: function _waitForLoad(aTab) {
    let browser = aTab.browser;

    aTab._firstPaint = false;

    browser.messageManager.addMessageListener("Browser:FirstPaint", function firstPaintListener(aMessage) {
      browser.messageManager.removeMessageListener(aMessage.name, arguments.callee);
      aTab._firstPaint = true;
      aTab.scrolledAreaChanged(true);
      aTab.updateThumbnailSource();
    });
  },

  _networkStart: function _networkStart(aJson, aTab) {
    aTab.startLoading();

    if (aTab == Browser.selectedTab) {
      BrowserUI.update(TOOLBARSTATE_LOADING);

      // We should at least show something in the URLBar until
      // the load has progressed further along
      if (aTab.browser.currentURI.spec == "about:blank")
        BrowserUI.updateURI({ captionOnly: true });
    }
  },

  _networkStop: function _networkStop(aJson, aTab) {
    aTab.endLoading();

    if (aTab == Browser.selectedTab) {
      BrowserUI.update(TOOLBARSTATE_LOADED);
    }
  },

  _windowStart: function _windowStart(aJson, aTab) {
    this._progressStart(aJson, aTab);
  },

  _windowStop: function _windowStop(aJson, aTab) {
    this._progressStop(aJson, aTab);
  },

  _progressStart: function _progressStart(aJson, aTab) {
    // We will get multiple calls from _windowStart, so
    // only process once.
    if (this._progressActive)
      return;

    this._progressActive = true;

    // display the track
    Elements.progressContainer.removeAttribute("collapsed");

    // 'Whoosh' in
    this._progressCount = kProgressMarginStart;
    Elements.progress.style.width = this._progressCount + "%"; 
    Elements.progress.removeAttribute("fade");

    // Create a pulse timer to keep things moving even if we don't
    // collect any state changes.
    setTimeout(function() {
      WebProgress._progressStepTimer();
    }, kHeartbeatDuration, this);
  },

  _stepProgressCount: function _stepProgressCount() {
    // Step toward the end margin in smaller slices as we get closer
    let left = kProgressMarginEnd - this._progressCount;
    let step = left * .05;
    this._progressCount += Math.ceil(step);

    // Don't go past the 'whoosh out' margin.
    if (this._progressCount > kProgressMarginEnd) {
      this._progressCount = kProgressMarginEnd;
    }
  },

  _progressStep: function _progressStep() {
    if (!this._progressActive)
      return;
    this._stepProgressCount();
    Elements.progress.style.width = this._progressCount + "%";
  },

  _progressStepTimer: function _progressStepTimer() {
    if (!this._progressActive)
      return;
    this._progressStep();

    setTimeout(function() {
      WebProgress._progressStepTimer();
    }, kHeartbeatDuration, this);
  },

  _progressStop: function _progressStop(aJson, aTab) {
    this._progressActive = false;
    // 'Whoosh out' and fade
    Elements.progress.style.width = "100%"; 
    Elements.progress.setAttribute("fade", true);
  },

  _progressTransEnd: function _progressTransEnd(data) {
    if (!Elements.progress.hasAttribute("fade"))
      return;
    // Close out fade finished, reset
    if (data.propertyName == "opacity") {
      Elements.progress.style.width = "0px"; 
      Elements.progressContainer.setAttribute("collapsed", true);
    }
  },
};


