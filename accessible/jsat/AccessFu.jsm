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
   * Initialize chrome-layer accessibility functionality.
   * If accessibility is enabled on the platform, then a special accessibility
   * mode is started.
   */
  attach: function attach(aWindow, aInTest = false) {
    Utils.init(aWindow);

    if (!aInTest) {
      this._enable();
    }
  },

  /**
   * Shut down chrome-layer accessibility functionality from the outside.
   */
  detach: function detach() {
    // Avoid disabling twice.
    if (this._enabled) {
      this._disable();
    }

    Utils.uninit();
  },

  /**
   * A lazy getter for event handler that binds the scope to AccessFu object.
   */
  get handleEvent() {
    delete this.handleEvent;
    this.handleEvent = this._handleEvent.bind(this);
    return this.handleEvent;
  },

  /**
   * Start AccessFu mode, this primarily means controlling the virtual cursor
   * with arrow keys.
   */
  _enable: function _enable() {
    if (this._enabled) {
      return;
    }
    this._enabled = true;

    ChromeUtils.import("resource://gre/modules/accessibility/Utils.jsm");
    ChromeUtils.import("resource://gre/modules/accessibility/Presentation.jsm");

    for (let mm of Utils.AllMessageManagers) {
      this._addMessageListeners(mm);
      this._loadFrameScript(mm);
    }

    // Check for output notification
    this._notifyOutputPref =
      new PrefCache("accessibility.accessfu.notify_output");

    if (Utils.MozBuildApp === "mobile/android") {
      Utils.win.WindowEventDispatcher.registerListener(this,
        Object.values(GECKOVIEW_MESSAGE));
    }

    Services.obs.addObserver(this, "remote-browser-shown");
    Services.obs.addObserver(this, "inprocess-browser-shown");
    Utils.win.addEventListener("TabOpen", this);
    Utils.win.addEventListener("TabClose", this);
    Utils.win.addEventListener("TabSelect", this);

    if (this.readyCallback) {
      this.readyCallback();
      delete this.readyCallback;
    }

    Logger.info("AccessFu:Enabled");
  },

  /**
   * Disable AccessFu and return to default interaction mode.
   */
  _disable: function _disable() {
    if (!this._enabled) {
      return;
    }

    this._enabled = false;

    for (let mm of Utils.AllMessageManagers) {
      mm.sendAsyncMessage("AccessFu:Stop");
      this._removeMessageListeners(mm);
    }

    Utils.win.removeEventListener("TabOpen", this);
    Utils.win.removeEventListener("TabClose", this);
    Utils.win.removeEventListener("TabSelect", this);

    Services.obs.removeObserver(this, "remote-browser-shown");
    Services.obs.removeObserver(this, "inprocess-browser-shown");

    if (Utils.MozBuildApp === "mobile/android") {
      Utils.win.WindowEventDispatcher.unregisterListener(this,
        Object.values(GECKOVIEW_MESSAGE));
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
        this.Input.doScroll(aMessage.json);
        break;
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

    for (let evt of aPresentationData) {
      if (typeof evt == "string") {
        continue;
      }

      Utils.win.WindowEventDispatcher.sendRequest({
        ...evt,
        type: "GeckoView:AccessibilityEvent"
      });
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
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:AutoMove", aOptions);
  },

  announce: function announce(aAnnouncement) {
    this._output(Presentation.announce(aAnnouncement), Utils.CurrentBrowser);
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
   */
  screenToClientBounds(aJsonBounds) {
      let bounds = new Rect(aJsonBounds.left, aJsonBounds.top,
                            aJsonBounds.right - aJsonBounds.left,
                            aJsonBounds.bottom - aJsonBounds.top);
      let win = Utils.win;
      let dpr = win.devicePixelRatio;

      bounds = bounds.scale(1 / dpr, 1 / dpr);
      bounds = bounds.translate(-win.mozInnerScreenX, -win.mozInnerScreenY);
      return bounds.expandToIntegers();
    }
};

var Input = {
  moveToPoint: function moveToPoint(aRule, aX, aY) {
    // XXX: Bug 1013408 - There is no alignment between the chrome window's
    // viewport size and the content viewport size in Android. This makes
    // sending mouse events beyond its bounds impossible.
    if (Utils.MozBuildApp === "mobile/android") {
      let mm = Utils.getMessageManager(Utils.CurrentBrowser);
      mm.sendAsyncMessage("AccessFu:MoveToPoint",
        {rule: aRule, x: aX, y: aY, origin: "top"});
    } else {
      let win = Utils.win;
      Utils.winUtils.sendMouseEvent("mousemove",
        aX - win.mozInnerScreenX, aY - win.mozInnerScreenY, 0, 0, 0);
    }
  },

  moveCursor: function moveCursor(aAction, aRule, aInputType, aAdjustRange) {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:MoveCursor",
                        { action: aAction, rule: aRule,
                          origin: "top", inputType: aInputType,
                          adjustRange: aAdjustRange });
  },

  androidScroll: function androidScroll(aDirection) {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:AndroidScroll",
                        { direction: aDirection, origin: "top" });
  },

  moveByGranularity: function moveByGranularity(aDetails) {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:MoveByGranularity", aDetails);
  },

  activateCurrent: function activateCurrent(aData, aActivateIfKey = false) {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
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
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:Scroll",
      {page: aPage, horizontal: aHorizontal, origin: "top"});
  },

  doScroll: function doScroll(aDetails) {
    let horizontal = aDetails.horizontal;
    let page = aDetails.page;
    let p = AccessFu.screenToClientBounds(aDetails.bounds).center();
    Utils.winUtils.sendWheelEvent(p.x, p.y,
      horizontal ? page : 0, horizontal ? 0 : page, 0,
      Utils.win.WheelEvent.DOM_DELTA_PAGE, 0, 0, 0, 0);
  }
};
AccessFu.Input = Input;
