/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals addMessageListener, sendAsyncMessage, addEventListener,
   removeEventListener */
"use strict";

var { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

var systemAppOrigin = (function () {
  let systemOrigin = "_";
  try {
    systemOrigin = Services.io.newURI(
      Services.prefs.getCharPref("b2g.system_manifest_url"), null, null)
      .prePath;
  } catch (e) {
    // Fall back to default value
  }
  return systemOrigin;
})();

var threshold = 25;
try {
  threshold = Services.prefs.getIntPref("ui.dragThresholdX");
} catch (e) {
  // Fall back to default value
}

var delay = 500;
try {
  delay = Services.prefs.getIntPref("ui.click_hold_context_menus.delay");
} catch (e) {
  // Fall back to default value
}

/**
 * Simulate touch events for platforms where they aren't generally available.
 * This frame script is managed by `simulator.js`.
 */
var simulator = {
  events: [
    "mousedown",
    "mousemove",
    "mouseup",
    "touchstart",
    "touchend",
    "mouseenter",
    "mouseover",
    "mouseout",
    "mouseleave"
  ],

  messages: [
    "TouchEventSimulator:Start",
    "TouchEventSimulator:Stop",
  ],

  contextMenuTimeout: null,

  init() {
    this.messages.forEach(msgName => {
      addMessageListener(msgName, this);
    });
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "TouchEventSimulator:Start":
        this.start();
        break;
      case "TouchEventSimulator:Stop":
        this.stop();
        break;
    }
  },

  start() {
    this.events.forEach(evt => {
      // Only listen trusted events to prevent messing with
      // event dispatched manually within content documents
      addEventListener(evt, this, true, false);
    });
    sendAsyncMessage("TouchEventSimulator:Started");
  },

  stop() {
    this.events.forEach(evt => {
      removeEventListener(evt, this, true);
    });
    sendAsyncMessage("TouchEventSimulator:Stopped");
  },

  handleEvent(evt) {
    // The gaia system window use an hybrid system even on the device which is
    // a mix of mouse/touch events. So let's not cancel *all* mouse events
    // if it is the current target.
    let content = this.getContent(evt.target);
    if (!content) {
      return;
    }
    let isSystemWindow = content.location.toString()
                                .startsWith(systemAppOrigin);

    // App touchstart & touchend should also be dispatched on the system app
    // to match on-device behavior.
    if (evt.type.startsWith("touch") && !isSystemWindow) {
      let sysFrame = content.realFrameElement;
      if (!sysFrame) {
        return;
      }
      let sysDocument = sysFrame.ownerDocument;
      let sysWindow = sysDocument.defaultView;

      let touchEvent = sysDocument.createEvent("touchevent");
      let touch = evt.touches[0] || evt.changedTouches[0];
      let point = sysDocument.createTouch(sysWindow, sysFrame, 0,
                                          touch.pageX, touch.pageY,
                                          touch.screenX, touch.screenY,
                                          touch.clientX, touch.clientY,
                                          1, 1, 0, 0);

      let touches = sysDocument.createTouchList(point);
      let targetTouches = touches;
      let changedTouches = touches;
      touchEvent.initTouchEvent(evt.type, true, true, sysWindow, 0,
                                false, false, false, false,
                                touches, targetTouches, changedTouches);
      sysFrame.dispatchEvent(touchEvent);
      return;
    }

    // Ignore all but real mouse event coming from physical mouse
    // (especially ignore mouse event being dispatched from a touch event)
    if (evt.button ||
        evt.mozInputSource != Ci.nsIDOMMouseEvent.MOZ_SOURCE_MOUSE ||
        evt.isSynthesized) {
      return;
    }

    let eventTarget = this.target;
    let type = "";
    switch (evt.type) {
      case "mouseenter":
      case "mouseover":
      case "mouseout":
      case "mouseleave":
        // Don't propagate events which are not related to touch events
        evt.stopPropagation();
        break;

      case "mousedown":
        this.target = evt.target;

        this.contextMenuTimeout =
          this.sendContextMenu(evt.target, evt.pageX, evt.pageY);

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
          addEventListener("click", this, true, false);
        }
        break;

      case "click":
        // Mouse events has been cancelled so dispatch a sequence
        // of events to where touchend has been fired
        evt.preventDefault();
        evt.stopImmediatePropagation();

        removeEventListener("click", this, true, false);

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

    let target = eventTarget || this.target;
    if (target && type) {
      this.sendTouchEvent(evt, target, type);
    }

    if (!isSystemWindow) {
      evt.preventDefault();
      evt.stopImmediatePropagation();
    }
  },

  fireMouseEvent(type, evt) {
    let content = this.getContent(evt.target);
    let utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    utils.sendMouseEvent(type, evt.clientX, evt.clientY, 0, 1, 0, true, 0,
                         Ci.nsIDOMMouseEvent.MOZ_SOURCE_TOUCH);
  },

  sendContextMenu(target, x, y) {
    let doc = target.ownerDocument;
    let evt = doc.createEvent("MouseEvent");
    evt.initMouseEvent("contextmenu", true, true, doc.defaultView,
                       0, x, y, x, y, false, false, false, false,
                       0, null);

    let content = this.getContent(target);
    let timeout = content.setTimeout((function contextMenu() {
      target.dispatchEvent(evt);
      this.cancelClick = true;
    }).bind(this), delay);

    return timeout;
  },

  sendTouchEvent(evt, target, name) {
    function clone(obj) {
      return Cu.cloneInto(obj, target);
    }
    // When running OOP b2g desktop, we need to send the touch events
    // using the mozbrowser api on the unwrapped frame.
    if (target.localName == "iframe" && target.mozbrowser === true) {
      if (name == "touchstart") {
        this.touchstartTime = Date.now();
      } else if (name == "touchend") {
        // If we have a "fast" tap, don't send a click as both will be turned
        // into a click and that breaks eg. checkboxes.
        if (Date.now() - this.touchstartTime < delay) {
          this.cancelClick = true;
        }
      }
      let unwrapped = XPCNativeWrapper.unwrap(target);
      unwrapped.sendTouchEvent(name, clone([0]),       // event type, id
                               clone([evt.clientX]),   // x
                               clone([evt.clientY]),   // y
                               clone([1]), clone([1]), // rx, ry
                               clone([0]), clone([0]), // rotation, force
                               1);                     // count
      return;
    }
    let document = target.ownerDocument;
    let content = this.getContent(target);
    if (!content) {
      return;
    }

    let touchEvent = document.createEvent("touchevent");
    let point = document.createTouch(content, target, 0,
                                     evt.pageX, evt.pageY,
                                     evt.screenX, evt.screenY,
                                     evt.clientX, evt.clientY,
                                     1, 1, 0, 0);

    let touches = document.createTouchList(point);
    let targetTouches = touches;
    let changedTouches = touches;
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
    let win = (target && target.ownerDocument)
      ? target.ownerDocument.defaultView
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
    let savedMetaViewportEnabled =
      Services.prefs.getBoolPref("dom.meta-viewport.enabled");
    if (!savedMetaViewportEnabled) {
      return 300;
    }

    let content = this.getContent(evt.target);
    if (!content) {
      return 0;
    }

    let utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);

    let allowZoom = {},
        minZoom = {},
        maxZoom = {},
        autoSize = {};

    utils.getViewportInfo(content.innerWidth, content.innerHeight, {},
                          allowZoom, minZoom, maxZoom, {}, {}, autoSize);

    // FIXME: On Safari and Chrome mobile platform, if the css property
    // touch-action set to none or manipulation would also suppress 300ms
    // delay. But Firefox didn't support this property now, we can't get
    // this value from utils.getVisitedDependentComputedStyle() to check
    // if we should suppress 300ms delay.
    if (!allowZoom.value ||                   // user-scalable = no
        minZoom.value === maxZoom.value ||    // minimum-scale = maximum-scale
        autoSize.value                        // width = device-width
    ) {
      return 0;
    } else {
      return 300;
    }
  }
};

simulator.init();
