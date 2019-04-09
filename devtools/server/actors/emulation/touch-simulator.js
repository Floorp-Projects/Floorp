/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global XPCNativeWrapper */

"use strict";

const { Services } = require("resource://gre/modules/Services.jsm");

loader.lazyRequireGetter(this, "InspectorUtils", "InspectorUtils");

var systemAppOrigin = (function() {
  let systemOrigin = "_";
  try {
    systemOrigin =
      Services.io.newURI(Services.prefs.getCharPref("b2g.system_manifest_url"))
                 .prePath;
  } catch (e) {
    // Fall back to default value
  }
  return systemOrigin;
})();

var threshold = Services.prefs.getIntPref("ui.dragThresholdX", 25);
var delay = Services.prefs.getIntPref("ui.click_hold_context_menus.delay", 500);

const kStateHover = 0x00000004; // NS_EVENT_STATE_HOVER

function TouchSimulator(simulatorTarget) {
  this.simulatorTarget = simulatorTarget;
}

/**
 * Simulate touch events for platforms where they aren't generally available.
 */
TouchSimulator.prototype = {
  events: [
    "mousedown",
    "mousemove",
    "mouseup",
    "touchstart",
    "touchend",
    "mouseenter",
    "mouseover",
    "mouseout",
    "mouseleave",
  ],

  contextMenuTimeout: null,

  simulatorTarget: null,

  enabled: false,

  start() {
    if (this.enabled) {
      // Simulator is already started
      return;
    }
    this.events.forEach(evt => {
      // Only listen trusted events to prevent messing with
      // event dispatched manually within content documents
      this.simulatorTarget.addEventListener(evt, this, true, false);
    });
    this.enabled = true;
  },

  stop() {
    if (!this.enabled) {
      // Simulator isn't running
      return;
    }
    this.events.forEach(evt => {
      this.simulatorTarget.removeEventListener(evt, this, true);
    });
    this.enabled = false;
  },

  /**
   * Set the current element picker state value.
   * True means the element picker is currently active and we should not be emulating
   * touch events.
   * False means the element picker is not active and it is ok to emulate touch events.
   * @param {Boolean} state
   */
  setElementPickerState(state) {
    this._isPicking = state;
  },

  handleEvent(evt) {
    // Bail out if devtools is in pick mode in the same tab.
    if (this._isPicking) {
      return;
    }

    // The gaia system window use an hybrid system even on the device which is
    // a mix of mouse/touch events. So let's not cancel *all* mouse events
    // if it is the current target.
    const content = this.getContent(evt.target);
    if (!content) {
      return;
    }
    const isSystemWindow = content.location.toString()
                                .startsWith(systemAppOrigin);

    // App touchstart & touchend should also be dispatched on the system app
    // to match on-device behavior.
    if (evt.type.startsWith("touch") && !isSystemWindow) {
      const sysFrame = content.realFrameElement;
      if (!sysFrame) {
        return;
      }
      const sysDocument = sysFrame.ownerDocument;
      const sysWindow = sysDocument.defaultView;

      const touchEvent = sysDocument.createEvent("touchevent");
      const touch = evt.touches[0] || evt.changedTouches[0];
      const point = sysDocument.createTouch(sysWindow, sysFrame, 0,
                                          touch.pageX, touch.pageY,
                                          touch.screenX, touch.screenY,
                                          touch.clientX, touch.clientY,
                                          1, 1, 0, 0);

      const touches = sysDocument.createTouchList(point);
      const targetTouches = touches;
      const changedTouches = touches;
      touchEvent.initTouchEvent(evt.type, true, true, sysWindow, 0,
                                false, false, false, false,
                                touches, targetTouches, changedTouches);
      sysFrame.dispatchEvent(touchEvent);
      return;
    }

    // Ignore all but real mouse event coming from physical mouse
    // (especially ignore mouse event being dispatched from a touch event)
    if (evt.button ||
        evt.mozInputSource != evt.MOZ_SOURCE_MOUSE ||
        evt.isSynthesized) {
      return;
    }

    const eventTarget = this.target;
    let type = "";
    switch (evt.type) {
      case "mouseenter":
      case "mouseover":
      case "mouseout":
      case "mouseleave":
        // Don't propagate events which are not related to touch events
        evt.stopPropagation();
        evt.preventDefault();

        // We don't want to trigger any visual changes to elements whose content can
        // be modified via hover states. We can avoid this by removing the element's
        // content state.
        InspectorUtils.removeContentState(evt.target, kStateHover);
        break;

      case "mousedown":
        this.target = evt.target;

        this.contextMenuTimeout = this.sendContextMenu(evt);

        this.cancelClick = false;
        this.startX = evt.pageX;
        this.startY = evt.pageY;

        // Capture events so if a different window show up the events
        // won't be dispatched to something else.
        evt.target.setCapture(false);

        type = "touchstart";
        break;

      case "mousemove":
        if (!eventTarget) {
          // Don't propagate mousemove event when touchstart event isn't fired
          evt.stopPropagation();
          return;
        }

        if (!this.cancelClick) {
          if (Math.abs(this.startX - evt.pageX) > threshold ||
              Math.abs(this.startY - evt.pageY) > threshold) {
            this.cancelClick = true;
            content.clearTimeout(this.contextMenuTimeout);
          }
        }

        type = "touchmove";
        break;

      case "mouseup":
        if (!eventTarget) {
          return;
        }
        this.target = null;

        content.clearTimeout(this.contextMenuTimeout);
        type = "touchend";

        // Only register click listener after mouseup to ensure
        // catching only real user click. (Especially ignore click
        // being dispatched on form submit)
        if (evt.detail == 1) {
          this.simulatorTarget.addEventListener("click", this, true, false);
        }
        break;

      case "click":
        // Mouse events has been cancelled so dispatch a sequence
        // of events to where touchend has been fired
        evt.preventDefault();
        evt.stopImmediatePropagation();

        this.simulatorTarget.removeEventListener("click", this, true, false);

        if (this.cancelClick) {
          return;
        }

        content.setTimeout(function dispatchMouseEvents(self) {
          try {
            self.fireMouseEvent("mousedown", evt);
            self.fireMouseEvent("mousemove", evt);
            self.fireMouseEvent("mouseup", evt);
          } catch (e) {
            console.error("Exception in touch event helper: " + e);
          }
        }, this.getDelayBeforeMouseEvent(evt), this);
        return;
    }

    const target = eventTarget || this.target;
    if (target && type) {
      this.sendTouchEvent(evt, target, type);
    }

    if (!isSystemWindow) {
      evt.preventDefault();
      evt.stopImmediatePropagation();
    }
  },

  fireMouseEvent(type, evt) {
    const content = this.getContent(evt.target);
    const utils = content.windowUtils;
    utils.sendMouseEvent(type, evt.clientX, evt.clientY, 0, 1, 0, true, 0,
                         evt.MOZ_SOURCE_TOUCH);
  },

  sendContextMenu({ target, clientX, clientY, screenX, screenY }) {
    const view = target.ownerGlobal;
    const { MouseEvent } = view;
    const evt = new MouseEvent("contextmenu", {
      bubbles: true,
      cancelable: true,
      view,
      screenX,
      screenY,
      clientX,
      clientY,
    });
    const content = this.getContent(target);
    const timeout = content.setTimeout(() => {
      target.dispatchEvent(evt);
      this.cancelClick = true;
    }, delay);

    return timeout;
  },

  sendTouchEvent(evt, target, name) {
    const document = target.ownerDocument;
    const content = this.getContent(target);
    if (!content) {
      return;
    }

    const touchEvent = document.createEvent("touchevent");
    const point = document.createTouch(content, target, 0,
                                     evt.pageX, evt.pageY,
                                     evt.screenX, evt.screenY,
                                     evt.clientX, evt.clientY,
                                     1, 1, 0, 0);

    let touches = document.createTouchList(point);
    let targetTouches = touches;
    const changedTouches = touches;
    if (name === "touchend" || name === "touchcancel") {
      // "touchend" and "touchcancel" events should not have the removed touch
      // neither in touches nor in targetTouches
      touches = targetTouches = document.createTouchList();
    }

    touchEvent.initTouchEvent(name, true, true, content, 0,
                              false, false, false, false,
                              touches, targetTouches, changedTouches);
    target.dispatchEvent(touchEvent);
  },

  getContent(target) {
    const win = (target && target.ownerDocument)
      ? target.ownerGlobal
      : null;
    return win;
  },

  getDelayBeforeMouseEvent(evt) {
    // On mobile platforms, Firefox inserts a 300ms delay between
    // touch events and accompanying mouse events, except if the
    // content window is not zoomable and the content window is
    // auto-zoomed to device-width.

    // If the preference dom.meta-viewport.enabled is set to false,
    // we couldn't read viewport's information from getViewportInfo().
    // So we always simulate 300ms delay when the
    // dom.meta-viewport.enabled is false.
    const savedMetaViewportEnabled =
      Services.prefs.getBoolPref("dom.meta-viewport.enabled");
    if (!savedMetaViewportEnabled) {
      return 300;
    }

    const content = this.getContent(evt.target);
    if (!content) {
      return 0;
    }

    const utils = content.windowUtils;

    const allowZoom = {};
    const minZoom = {};
    const maxZoom = {};
    const autoSize = {};

    utils.getViewportInfo(content.innerWidth, content.innerHeight, {},
                          allowZoom, minZoom, maxZoom, {}, {}, autoSize);

    // FIXME: On Safari and Chrome mobile platform, if the css property
    // touch-action set to none or manipulation would also suppress 300ms
    // delay. But Firefox didn't support this property now, we can't get
    // this value from utils.getVisitedDependentComputedStyle() to check
    // if we should suppress 300ms delay.
    if (!allowZoom.value || // user-scalable = no
        minZoom.value === maxZoom.value || // minimum-scale = maximum-scale
        autoSize.value // width = device-width
    ) {
      return 0;
    }
    return 300;
  },
};

exports.TouchSimulator = TouchSimulator;
