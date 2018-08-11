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
  BY_GRANULARITY: "GeckoView:AccessibilityByGranularity",
  CLIPBOARD: "GeckoView:AccessibilityClipboard",
  EXPLORE_BY_TOUCH: "GeckoView:AccessibilityExploreByTouch",
  LONG_PRESS: "GeckoView:AccessibilityLongPress",
  NEXT: "GeckoView:AccessibilityNext",
  PREVIOUS: "GeckoView:AccessibilityPrevious",
  SCROLL_BACKWARD: "GeckoView:AccessibilityScrollBackward",
  SCROLL_FORWARD: "GeckoView:AccessibilityScrollForward",
  SELECT: "GeckoView:AccessibilitySelect",
  SET_SELECTION: "GeckoView:AccessibilitySetSelection",
  VIEW_FOCUSED: "GeckoView:AccessibilityViewFocused",
};

const ACCESSFU_MESSAGE = {
  PRESENT: "AccessFu:Present",
  DOSCROLL: "AccessFu:DoScroll",
};

const FRAME_SCRIPT = "chrome://global/content/accessibility/content-script.js";

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
      case ACCESSFU_MESSAGE.PRESENT:
        this._output(aMessage.json, aMessage.target);
        break;
      case ACCESSFU_MESSAGE.DOSCROLL:
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

    // Set up frame script
    let mm = win.messageManager;
    for (let messageName of Object.values(ACCESSFU_MESSAGE)) {
      mm.addMessageListener(messageName, this);
    }
    mm.loadFrameScript(FRAME_SCRIPT, true);

    win.addEventListener("TabSelect", this);
    if (win.WindowEventDispatcher) {
      // desktop mochitests don't have this.
      win.WindowEventDispatcher.registerListener(this,
        Object.values(GECKOVIEW_MESSAGE));
    }
  },

  _detachWindow: function _detachWindow(win) {
    let mm = win.messageManager;
    mm.broadcastAsyncMessage("AccessFu:Stop");
    mm.removeDelayedFrameScript(FRAME_SCRIPT);
    for (let messageName of Object.values(ACCESSFU_MESSAGE)) {
      mm.removeMessageListener(messageName, this);
    }

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
      case GECKOVIEW_MESSAGE.SET_SELECTION:
        this.Input.setSelection(data);
        break;
      case GECKOVIEW_MESSAGE.CLIPBOARD:
        this.Input.clipboard(data);
        break;
      case GECKOVIEW_MESSAGE.SELECT:
        this.Input.selectCurrent(data);
        break;
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "domwindowopened": {
        let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
        win.addEventListener("load", () => {
          this._attachWindow(win);
        }, { once: true });
        break;
      }
    }
  },

  _handleEvent: function _handleEvent(aEvent) {
    switch (aEvent.type) {
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
    const mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:AutoMove", aOptions);
  },

  announce: function announce(aAnnouncement) {
    this._output(Presentation.announce(aAnnouncement), Utils.getCurrentBrowser());
  },

  // So we don't enable/disable twice
  _enabled: false,

  // Layerview is focused
  _focused: false,

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
    const mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:MoveToPoint",
      {rule: aRule, x: aX, y: aY, origin: "top"});
  },

  moveCursor: function moveCursor(aAction, aRule, aInputType, aAdjustRange) {
    const mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:MoveCursor",
                        { action: aAction, rule: aRule,
                          origin: "top", inputType: aInputType,
                          adjustRange: aAdjustRange });
  },

  androidScroll: function androidScroll(aDirection) {
    const mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:AndroidScroll",
                        { direction: aDirection, origin: "top" });
  },

  moveByGranularity: function moveByGranularity(aDetails) {
    const mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:MoveByGranularity", aDetails);
  },

  setSelection: function setSelection(aDetails) {
    const mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:SetSelection", aDetails);
  },

  clipboard: function clipboard(aDetails) {
    const mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:Clipboard", aDetails);
  },

  activateCurrent: function activateCurrent(aData) {
    let mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:Activate", { offset: 0 });
  },

  selectCurrent: function selectCurrent(aData) {
    let mm = Utils.getMessageManager();
    mm.sendAsyncMessage("AccessFu:Select", aData);
  },

  doScroll: function doScroll(aDetails, aBrowser) {
    let horizontal = aDetails.horizontal;
    let page = aDetails.page;
    let win = aBrowser.ownerGlobal;
    let winUtils = win.windowUtils;
    let p = AccessFu.screenToClientBounds(aDetails.bounds, win).center();
    winUtils.sendWheelEvent(p.x, p.y,
      horizontal ? page : 0, horizontal ? 0 : page, 0,
      win.WheelEvent.DOM_DELTA_PAGE, 0, 0, 0, 0);
  }
};
AccessFu.Input = Input;
