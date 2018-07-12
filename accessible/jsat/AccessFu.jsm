/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AccessFu"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "Rect",
                               "resource://gre/modules/Geometry.jsm");

if (Utils.MozBuildApp === "mobile/android") {
  ChromeUtils.import("resource://gre/modules/Messaging.jsm");
}

const GECKOVIEW_MESSAGE = {
  ACTIVATE: "GeckoView:AccessibilityActivate",
  VIEW_FOCUSED: "GeckoView:AccessibilityViewFocused",
  LONG_PRESS: "GeckoView:AccessibilityLongPress",
  BY_GRANULARITY: "GeckoView:AccessibilityByGranularity",
  NEXT: "GeckoView:AccessibilityNext",
  PREVIOUS: "GeckoView:AccessibilityPrevious",
  SCROLL_BACKWARD: "GeckoView:AccessibilityScrollBackward",
  SCROLL_FORWARD: "GeckoView:AccessibilityScrollForward",
  EXPLORE_BY_TOUCH: "GeckoView:AccessibilityExploreByTouch"
};

var AccessFu = {
  /**
   * A lazy getter for event handler that binds the scope to AccessFu object.
   */
  get handleEvent() {
    delete this.handleEvent;
    this.handleEvent = this._handleEvent.bind(this);
    return this.handleEvent;
  },

  /**
   * Start AccessFu mode.
   */
  enable: function enable() {
    if (this._enabled) {
      return;
    }
    this._enabled = true;

    ChromeUtils.import("resource://gre/modules/accessibility/Utils.jsm");
    ChromeUtils.import("resource://gre/modules/accessibility/Presentation.jsm");

    // Check for output notification
    this._notifyOutputPref =
      new PrefCache("accessibility.accessfu.notify_output");

    Services.obs.addObserver(this, "remote-browser-shown");
    Services.obs.addObserver(this, "inprocess-browser-shown");
    Services.ww.registerNotification(this);

    let windows = Services.wm.getEnumerator(null);
    while (windows.hasMoreElements()) {
      this._attachWindow(windows.getNext());
    }

    if (this.readyCallback) {
      this.readyCallback();
      delete this.readyCallback;
    }

    Logger.info("AccessFu:Enabled");
  },

  /**
   * Disable AccessFu and return to default interaction mode.
   */
  disable: function disable() {
    if (!this._enabled) {
      return;
    }

    this._enabled = false;

    Services.obs.removeObserver(this, "remote-browser-shown");
    Services.obs.removeObserver(this, "inprocess-browser-shown");
    Services.ww.unregisterNotification(this);

    let windows = Services.wm.getEnumerator(null);
    while (windows.hasMoreElements()) {
      this._detachWindow(windows.getNext());
    }

    delete this._notifyOutputPref;

    if (this.doneCallback) {
      this.doneCallback();
      delete this.doneCallback;
    }

    Logger.info("AccessFu:Disabled");
  },

  receiveMessage: function receiveMessage(aMessage) {
    Logger.debug(() => {
      return ["Recieved", aMessage.name, JSON.stringify(aMessage.json)];
    });

    switch (aMessage.name) {
      case "AccessFu:Ready":
        let mm = Utils.getMessageManager(aMessage.target);
        if (this._enabled) {
          mm.sendAsyncMessage("AccessFu:Start",
                              {method: "start", buildApp: Utils.MozBuildApp});
        }
        break;
      case "AccessFu:Present":
        this._output(aMessage.json, aMessage.target);
        break;
      case "AccessFu:DoScroll":
        this.Input.doScroll(aMessage.json, aMessage.target);
        break;
    }
  },

  _attachWindow: function _attachWindow(win) {
    let wtype = win.document.documentElement.getAttribute("windowtype");
    if (wtype != "navigator:browser" && wtype != "navigator:geckoview") {
      // Don't attach to non-browser or geckoview windows.
      return;
    }

    for (let mm of Utils.getAllMessageManagers(win)) {
      this._addMessageListeners(mm);
      this._loadFrameScript(mm);
    }

    win.addEventListener("TabOpen", this);
    win.addEventListener("TabClose", this);
    win.addEventListener("TabSelect", this);
    if (win.WindowEventDispatcher) {
      // desktop mochitests don't have this.
      win.WindowEventDispatcher.registerListener(this,
        Object.values(GECKOVIEW_MESSAGE));
    }
  },

  _detachWindow: function _detachWindow(win) {
    for (let mm of Utils.getAllMessageManagers(win)) {
      mm.sendAsyncMessage("AccessFu:Stop");
      this._removeMessageListeners(mm);
    }

    win.removeEventListener("TabOpen", this);
    win.removeEventListener("TabClose", this);
    win.removeEventListener("TabSelect", this);
    if (win.WindowEventDispatcher) {
      // desktop mochitests don't have this.
      win.WindowEventDispatcher.unregisterListener(this,
        Object.values(GECKOVIEW_MESSAGE));
    }
  },

  _output: function _output(aPresentationData, aBrowser) {
    if (!aPresentationData) {
      // Either no android events to send or a string used for testing only.
      return;
    }

    if (!Utils.isAliveAndVisible(Utils.AccService.getAccessibleFor(aBrowser))) {
      return;
    }

    let win = aBrowser.ownerGlobal;

    for (let evt of aPresentationData) {
      if (typeof evt == "string") {
        continue;
      }

      if (win.WindowEventDispatcher) {
        // desktop mochitests don't have this.
        win.WindowEventDispatcher.sendRequest({
          ...evt,
          type: "GeckoView:AccessibilityEvent"
        });
      }
    }

    if (this._notifyOutputPref.value) {
      Services.obs.notifyObservers(null, "accessibility-output",
                                   JSON.stringify(aPresentationData));
    }
  },

  _loadFrameScript: function _loadFrameScript(aMessageManager) {
    if (!this._processedMessageManagers.includes(aMessageManager)) {
      aMessageManager.loadFrameScript(
        "chrome://global/content/accessibility/content-script.js", true);
      this._processedMessageManagers.push(aMessageManager);
    } else if (this._enabled) {
      // If the content-script is already loaded and AccessFu is enabled,
      // send an AccessFu:Start message.
      aMessageManager.sendAsyncMessage("AccessFu:Start",
        {method: "start", buildApp: Utils.MozBuildApp});
    }
  },

  _addMessageListeners: function _addMessageListeners(aMessageManager) {
    aMessageManager.addMessageListener("AccessFu:Present", this);
    aMessageManager.addMessageListener("AccessFu:Ready", this);
    aMessageManager.addMessageListener("AccessFu:DoScroll", this);
  },

  _removeMessageListeners: function _removeMessageListeners(aMessageManager) {
    aMessageManager.removeMessageListener("AccessFu:Present", this);
    aMessageManager.removeMessageListener("AccessFu:Ready", this);
    aMessageManager.removeMessageListener("AccessFu:DoScroll", this);
  },

  _handleMessageManager: function _handleMessageManager(aMessageManager) {
    if (this._enabled) {
      this._addMessageListeners(aMessageManager);
    }
    this._loadFrameScript(aMessageManager);
  },

  onEvent(event, data, callback) {
    switch (event) {
      case GECKOVIEW_MESSAGE.SETTINGS:
        if (data.enabled) {
          this._enable();
        } else {
          this._disable();
        }
        break;
      case GECKOVIEW_MESSAGE.NEXT:
      case GECKOVIEW_MESSAGE.PREVIOUS: {
        let rule = "Simple";
        if (data && data.rule && data.rule.length) {
          rule = data.rule.substr(0, 1).toUpperCase() +
            data.rule.substr(1).toLowerCase();
        }
        let method = event.replace(/GeckoView:Accessibility(\w+)/, "move$1");
        this.Input.moveCursor(method, rule, "gesture");
        break;
      }
      case GECKOVIEW_MESSAGE.ACTIVATE:
        this.Input.activateCurrent(data);
        break;
      case GECKOVIEW_MESSAGE.LONG_PRESS:
        // XXX: Advertize long press on supported objects and implement action
        break;
      case GECKOVIEW_MESSAGE.SCROLL_FORWARD:
        this.Input.androidScroll("forward");
        break;
      case GECKOVIEW_MESSAGE.SCROLL_BACKWARD:
        this.Input.androidScroll("backward");
        break;
      case GECKOVIEW_MESSAGE.VIEW_FOCUSED:
        this._focused = data.gainFocus;
        if (this._focused) {
          this.autoMove({ forcePresent: true, noOpIfOnScreen: true });
        }
        break;
      case GECKOVIEW_MESSAGE.BY_GRANULARITY:
        this.Input.moveByGranularity(data);
        break;
      case GECKOVIEW_MESSAGE.EXPLORE_BY_TOUCH:
        this.Input.moveToPoint("Simple", ...data.coordinates);
        break;
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "remote-browser-shown":
      case "inprocess-browser-shown":
      {
        // Ignore notifications that aren't from a Browser
        let frameLoader = aSubject;
        if (!frameLoader.ownerIsMozBrowserFrame) {
          return;
        }
        this._handleMessageManager(frameLoader.messageManager);
        break;
      }
      case "domwindowopened": {
        this._attachWindow(aSubject.QueryInterface(Ci.nsIDOMWindow));
        break;
      }
    }
  },

  _handleEvent: function _handleEvent(aEvent) {
    switch (aEvent.type) {
      case "TabOpen":
      {
        let mm = Utils.getMessageManager(aEvent.target);
        this._handleMessageManager(mm);
        break;
      }
      case "TabClose":
      {
        let mm = Utils.getMessageManager(aEvent.target);
        let mmIndex = this._processedMessageManagers.indexOf(mm);
        if (mmIndex > -1) {
          this._removeMessageListeners(mm);
          this._processedMessageManagers.splice(mmIndex, 1);
        }
        break;
      }
      case "TabSelect":
      {
        if (this._focused) {
          // We delay this for half a second so the awesomebar could close,
          // and we could use the current coordinates for the content item.
          // XXX TODO figure out how to avoid magic wait here.
          this.autoMove({
            delay: 500,
            forcePresent: true,
            noOpIfOnScreen: true,
            moveMethod: "moveFirst" });
        }
        break;
      }
      default:
        break;
    }
  },

  autoMove: function autoMove(aOptions) {
    let mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:AutoMove", aOptions);
  },

  announce: function announce(aAnnouncement) {
    this._output(Presentation.announce(aAnnouncement), Utils.getCurrentBrowser());
  },

  // So we don't enable/disable twice
  _enabled: false,

  // Layerview is focused
  _focused: false,

  // Keep track of message managers tha already have a 'content-script.js'
  // injected.
  _processedMessageManagers: [],

  /**
   * Adjusts the given bounds that are defined in device display pixels
   * to client-relative CSS pixels of the chrome window.
   * @param {Rect} aJsonBounds the bounds to adjust
   * @param {Window} aWindow the window containing the item
   */
  screenToClientBounds(aJsonBounds, aWindow) {
      let bounds = new Rect(aJsonBounds.left, aJsonBounds.top,
                            aJsonBounds.right - aJsonBounds.left,
                            aJsonBounds.bottom - aJsonBounds.top);
      let { devicePixelRatio, mozInnerScreenX, mozInnerScreenY } = aWindow;

      bounds = bounds.scale(1 / devicePixelRatio, 1 / devicePixelRatio);
      bounds = bounds.translate(-mozInnerScreenX, -mozInnerScreenY);
      return bounds.expandToIntegers();
    }
};

var Input = {
  moveToPoint: function moveToPoint(aRule, aX, aY) {
    let mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:MoveToPoint",
      {rule: aRule, x: aX, y: aY, origin: "top"});
  },

  moveCursor: function moveCursor(aAction, aRule, aInputType, aAdjustRange) {
    let mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:MoveCursor",
                        { action: aAction, rule: aRule,
                          origin: "top", inputType: aInputType,
                          adjustRange: aAdjustRange });
  },

  androidScroll: function androidScroll(aDirection) {
    let mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:AndroidScroll",
                        { direction: aDirection, origin: "top" });
  },

  moveByGranularity: function moveByGranularity(aDetails) {
    let mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:MoveByGranularity", aDetails);
  },

  activateCurrent: function activateCurrent(aData, aActivateIfKey = false) {
    let mm = Utils.getMessageManager();
    let offset = 0;

    mm.sendAsyncMessage("AccessFu:Activate",
                        {offset, activateIfKey: aActivateIfKey});
  },

  // XXX: This is here for backwards compatability with screen reader simulator
  // it should be removed when the extension is updated on amo.
  scroll: function scroll(aPage, aHorizontal) {
    this.sendScrollMessage(aPage, aHorizontal);
  },

  sendScrollMessage: function sendScrollMessage(aPage, aHorizontal) {
    let mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:Scroll",
      {page: aPage, horizontal: aHorizontal, origin: "top"});
  },

  doScroll: function doScroll(aDetails, aBrowser) {
    let horizontal = aDetails.horizontal;
    let page = aDetails.page;
    let win = aBrowser.ownerGlobal;
    let winUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(
      Ci.nsIDOMWindowUtils);
    let p = AccessFu.screenToClientBounds(aDetails.bounds, win).center();
    winUtils.sendWheelEvent(p.x, p.y,
      horizontal ? page : 0, horizontal ? 0 : page, 0,
      win.WheelEvent.DOM_DELTA_PAGE, 0, 0, 0, 0);
  }
};
AccessFu.Input = Input;
