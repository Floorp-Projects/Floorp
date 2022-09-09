/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "InspectorUtils", "InspectorUtils");
loader.lazyRequireGetter(
  this,
  "PICKER_TYPES",
  "devtools/shared/picker-constants"
);

var isClickHoldEnabled = Services.prefs.getBoolPref(
  "ui.click_hold_context_menus"
);
var clickHoldDelay = Services.prefs.getIntPref(
  "ui.click_hold_context_menus.delay",
  500
);

// Touch state constants are derived from values defined in: nsIDOMWindowUtils.idl
const TOUCH_CONTACT = 0x02;
const TOUCH_REMOVE = 0x04;

const TOUCH_STATES = {
  touchstart: TOUCH_CONTACT,
  touchmove: TOUCH_CONTACT,
  touchend: TOUCH_REMOVE,
};

const EVENTS_TO_HANDLE = [
  "mousedown",
  "mousemove",
  "mouseup",
  "touchstart",
  "touchend",
  "mouseenter",
  "mouseover",
  "mouseout",
  "mouseleave",
];

const kStateHover = 0x00000004; // ElementState::HOVER

/**
 * Simulate touch events for platforms where they aren't generally available.
 */
class TouchSimulator {
  /**
   * @param {ChromeEventHandler} simulatorTarget: The object we'll use to listen for click
   *                             and touch events to handle.
   */
  constructor(simulatorTarget) {
    this.simulatorTarget = simulatorTarget;
    this._currentPickerMap = new Map();
  }

  enabled = false;

  start() {
    if (this.enabled) {
      // Simulator is already started
      return;
    }

    EVENTS_TO_HANDLE.forEach(evt => {
      // Only listen trusted events to prevent messing with
      // event dispatched manually within content documents
      this.simulatorTarget.addEventListener(evt, this, true, false);
    });

    this.enabled = true;
  }

  stop() {
    if (!this.enabled) {
      // Simulator isn't running
      return;
    }
    EVENTS_TO_HANDLE.forEach(evt => {
      this.simulatorTarget.removeEventListener(evt, this, true);
    });
    this.enabled = false;
  }

  _isPicking() {
    const types = Object.values(PICKER_TYPES);
    return types.some(type => this._currentPickerMap.get(type));
  }

  /**
   * Set the state value for one of DevTools pickers (either eyedropper or
   * element picker).
   * If any content picker is currently active, we should not be emulating
   * touch events. Otherwise it is ok to emulate touch events.
   * In theory only one picker can ever be active at a time, but tracking the
   * different pickers independantly avoids race issues in the client code.
   *
   * @param {Boolean} state
   *        True if the picker is currently active, false otherwise.
   * @param {String} pickerType
   *        One of PICKER_TYPES.
   */
  setElementPickerState(state, pickerType) {
    if (!Object.values(PICKER_TYPES).includes(pickerType)) {
      throw new Error(
        "Unsupported type in setElementPickerState: " + pickerType
      );
    }
    this._currentPickerMap.set(pickerType, state);
  }

  // eslint-disable-next-line complexity
  handleEvent(evt) {
    // Bail out if devtools is in pick mode in the same tab.
    if (this._isPicking()) {
      return;
    }

    const content = this.getContent(evt.target);
    if (!content) {
      return;
    }

    // App touchstart & touchend should also be dispatched on the system app
    // to match on-device behavior.
    if (evt.type.startsWith("touch")) {
      const sysFrame = content.realFrameElement;
      if (!sysFrame) {
        return;
      }
      const sysDocument = sysFrame.ownerDocument;
      const sysWindow = sysDocument.defaultView;

      const touchEvent = sysDocument.createEvent("touchevent");
      const touch = evt.touches[0] || evt.changedTouches[0];
      const point = sysDocument.createTouch(
        sysWindow,
        sysFrame,
        0,
        touch.pageX,
        touch.pageY,
        touch.screenX,
        touch.screenY,
        touch.clientX,
        touch.clientY,
        1,
        1,
        0,
        0
      );

      const touches = sysDocument.createTouchList(point);
      const targetTouches = touches;
      const changedTouches = touches;
      touchEvent.initTouchEvent(
        evt.type,
        true,
        true,
        sysWindow,
        0,
        false,
        false,
        false,
        false,
        touches,
        targetTouches,
        changedTouches
      );
      sysFrame.dispatchEvent(touchEvent);
      return;
    }

    // Ignore all but real mouse event coming from physical mouse
    // (especially ignore mouse event being dispatched from a touch event)
    if (
      evt.button ||
      evt.mozInputSource != evt.MOZ_SOURCE_MOUSE ||
      evt.isSynthesized
    ) {
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

        // If the click-hold feature is enabled, start a timeout to convert long clicks
        // into contextmenu events.
        // Just don't do it if the event occurred on a scrollbar.
        if (isClickHoldEnabled && !evt.originalTarget.closest("scrollbar")) {
          this._contextMenuTimeout = this.sendContextMenu(evt);
        }

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

        type = "touchmove";
        break;

      case "mouseup":
        if (!eventTarget) {
          return;
        }
        this.target = null;

        content.clearTimeout(this._contextMenuTimeout);
        type = "touchend";

        // Only register click listener after mouseup to ensure
        // catching only real user click. (Especially ignore click
        // being dispatched on form submit)
        if (evt.detail == 1) {
          this.simulatorTarget.addEventListener("click", this, {
            capture: true,
            once: true,
          });
        }
        break;
    }

    const target = eventTarget || this.target;
    if (target && type) {
      this.synthesizeNativeTouch(content, evt.screenX, evt.screenY, type);
    }

    evt.preventDefault();
    evt.stopImmediatePropagation();
  }

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
    }, clickHoldDelay);

    return timeout;
  }

  /**
   * Synthesizes a native touch action on a given target element.
   *
   * @param {Window} win
   *        The target window.
   * @param {Number} screenX
   *        The `x` screen coordinate relative to the screen origin.
   * @param {Number} screenY
   *        The `y` screen coordinate relative to the screen origin.
   * @param {String} type
   *        A key appearing in the TOUCH_STATES associative array.
   */
  synthesizeNativeTouch(win, screenX, screenY, type) {
    // Native events work in device pixels.
    const utils = win.windowUtils;
    const deviceScale = win.devicePixelRatio;
    const pt = { x: screenX * deviceScale, y: screenY * deviceScale };

    utils.sendNativeTouchPoint(0, TOUCH_STATES[type], pt.x, pt.y, 1, 90, null);
    return true;
  }

  getContent(target) {
    const win = target?.ownerDocument ? target.ownerGlobal : null;
    return win;
  }
}

exports.TouchSimulator = TouchSimulator;
