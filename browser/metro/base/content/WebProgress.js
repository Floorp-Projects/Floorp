/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Progress heartbeat timer duration (ms)
const kHeartbeatDuration = 1000;
// Start and end progress screen css margins as percentages
const kProgressMarginStart = 30;
const kProgressMarginEnd = 70;

const WebProgress = {
  get _identityBox() { return document.getElementById("identity-box"); },

  init: function init() {
    messageManager.addMessageListener("Content:StateChange", this);
    messageManager.addMessageListener("Content:LocationChange", this);
    messageManager.addMessageListener("Content:SecurityChange", this);

    Elements.progress.addEventListener("transitionend", this, true);
    Elements.tabList.addEventListener("TabSelect", this, true);

    let urlBar = document.getElementById("urlbar-edit");
    urlBar.addEventListener("input", this, false);

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

        this._progressStep(tab);
        break;
      }

      case "Content:LocationChange": {
        this._locationChange(json, tab);
        this._progressStep(tab);
        break;
      }

      case "Content:SecurityChange": {
        this._securityChange(json, tab);
        this._progressStep(tab);
        break;
      }
    }
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "transitionend":
        this._progressTransEnd(aEvent);
        break;
      case "TabSelect":
        this._onTabSelect(aEvent);
        break;
      case "input":
        this._onUrlBarInput(aEvent);
        break;
    }
  },

  _securityChange: function _securityChange(aJson, aTab) {
    let state = aJson.state;
    let nsIWebProgressListener = Ci.nsIWebProgressListener;

    if (state & nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL) {
      aTab._identityState = "verifiedIdentity";
    } else if (state & nsIWebProgressListener.STATE_IS_SECURE) {
      aTab._identityState = "verifiedDomain";
    } else {
      aTab._identityState = "";
    }

    if (aTab == Browser.selectedTab) {
      this._identityBox.className = aTab._identityState;
    }
  },

  _locationChange: function _locationChange(aJson, aTab) {
    let spec = aJson.location;
    let location = spec.split("#")[0]; // Ignore fragment identifier changes.

    if (aTab == Browser.selectedTab) {
      BrowserUI.updateURI();
      BrowserUI.update();
      BrowserUI.updateStartURIAttributes(aJson.location);
    }

    let locationHasChanged = (location != aTab.browser.lastLocation);
    if (locationHasChanged) {
      Browser.getNotificationBox(aTab.browser).removeTransientNotifications();
      aTab.browser.lastLocation = location;
      aTab.browser.userTypedValue = "";
      aTab.browser.appIcon = { href: null, size:-1 };

#ifdef MOZ_CRASHREPORTER
      if (CrashReporter.enabled)
        CrashReporter.annotateCrashReport("URL", spec);
#endif
    }

    let event = document.createEvent("UIEvents");
    event.initUIEvent("URLChanged", true, false, window, locationHasChanged);
    aTab.browser.dispatchEvent(event);
  },

  _networkStart: function _networkStart(aJson, aTab) {
    aTab.startLoading();

    if (aTab == Browser.selectedTab) {
      // NO_STARTUI_VISIBILITY since the current uri for the tab has not
      // been updated yet. If we're coming off of the start page, this
      // would briefly show StartUI until _locationChange is called.
      BrowserUI.update(BrowserUI.NO_STARTUI_VISIBILITY);
    }
  },

  _networkStop: function _networkStop(aJson, aTab) {
    aTab.endLoading();

    if (aTab == Browser.selectedTab) {
      BrowserUI.update();
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
    if (aTab._progressActive)
      return;

    aTab._progressActive = true;

    // 'Whoosh' in
    aTab._progressCount = kProgressMarginStart;
    this._showProgressBar(aTab);
  },

  _showProgressBar: function (aTab) {
    // display the track
    if (aTab == Browser.selectedTab) {
      Elements.progressContainer.removeAttribute("collapsed");
      Elements.progress.style.width = aTab._progressCount + "%";
      Elements.progress.removeAttribute("fade");
    }

    // Create a pulse timer to keep things moving even if we don't
    // collect any state changes.
    setTimeout(function() {
      WebProgress._progressStepTimer(aTab);
    }, kHeartbeatDuration, this);
  },

  _stepProgressCount: function _stepProgressCount(aTab) {
    // Step toward the end margin in smaller slices as we get closer
    let left = kProgressMarginEnd - aTab._progressCount;
    let step = left * .05;
    aTab._progressCount += Math.ceil(step);

    // Don't go past the 'whoosh out' margin.
    if (aTab._progressCount > kProgressMarginEnd) {
      aTab._progressCount = kProgressMarginEnd;
    }
  },

  _progressStep: function _progressStep(aTab) {
    if (!aTab._progressActive)
      return;
    this._stepProgressCount(aTab);
    if (aTab == Browser.selectedTab) {
      Elements.progress.style.width = aTab._progressCount + "%";
    }
  },

  _progressStepTimer: function _progressStepTimer(aTab) {
    if (!aTab._progressActive)
      return;
    this._progressStep(aTab);

    setTimeout(function() {
      WebProgress._progressStepTimer(aTab);
    }, kHeartbeatDuration, this);
  },

  _progressStop: function _progressStop(aJson, aTab) {
    aTab._progressActive = false;
    // 'Whoosh out' and fade
    if (aTab == Browser.selectedTab) {
      Elements.progress.style.width = "100%";
      Elements.progress.setAttribute("fade", true);
    }
  },

  _progressTransEnd: function _progressTransEnd(aEvent) {
    if (!Elements.progress.hasAttribute("fade"))
      return;
    // Close out fade finished, reset
    if (aEvent.propertyName == "opacity") {
      Elements.progress.style.width = "0px";
      Elements.progressContainer.setAttribute("collapsed", true);
    }
  },

  _onTabSelect: function(aEvent) {
    let tab = Browser.getTabFromChrome(aEvent.originalTarget);
    this._identityBox.className = tab._identityState || "";
    if (tab._progressActive) {
      this._showProgressBar(tab);
    } else {
      Elements.progress.setAttribute("fade", true);
      Elements.progressContainer.setAttribute("collapsed", true);
    }
  },

  _onUrlBarInput: function(aEvent) {
    Browser.selectedTab._identityState = this._identityBox.className = "";
  },
};
