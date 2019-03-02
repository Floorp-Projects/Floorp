/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

exports.getAvailableEventBreakpoints = getAvailableEventBreakpoints;
function getAvailableEventBreakpoints() {
  const available = [];
  for (const { name, items } of AVAILABLE_BREAKPOINTS) {
    available.push({
      name,
      events: items.map(item => ({
        id: item.id,
        name: item.eventType,
      })),
    });
  }
  return available;
}

function event(groupID, name, filter = "content") {
  return {
    id: `${groupID}.event.${name}`,
    type: "event",
    eventType: name,
    filter,
  };
}

const AVAILABLE_BREAKPOINTS = [
  {
    name: "Clipboard",
    items: [
      event("clipboard", "copy"),
      event("clipboard", "cut"),
      event("clipboard", "paste"),
      event("clipboard", "beforecopy"),
      event("clipboard", "beforecut"),
      event("clipboard", "beforepaste"),
    ],
  },
  {
    name: "Control",
    items: [
      event("control", "resize"),
      event("control", "scroll"),
      event("control", "zoom"),
      event("control", "focus"),
      event("control", "blur"),
      event("control", "select"),
      event("control", "change"),
      event("control", "submit"),
      event("control", "reset"),
    ],
  },
  {
    name: "DOM Mutation",
    items: [
      // Deprecated DOM events.
      event("dom-mutation", "DOMActivate"),
      event("dom-mutation", "DOMFocusIn"),
      event("dom-mutation", "DOMFocusOut"),

      // Standard DOM mutation events.
      event("dom-mutation", "DOMAttrModified"),
      event("dom-mutation", "DOMCharacterDataModified"),
      event("dom-mutation", "DOMNodeInserted"),
      event("dom-mutation", "DOMNodeInsertedIntoDocument"),
      event("dom-mutation", "DOMNodeRemoved"),
      event("dom-mutation", "DOMNodeRemovedIntoDocument"),
      event("dom-mutation", "DOMSubtreeModified"),

      // DOM load events.
      event("dom-mutation", "DOMContentLoaded"),
    ],
  },
  {
    name: "Device",
    items: [
      event("device", "deviceorientation"),
      event("device", "devicemotion"),
    ],
  },
  {
    name: "Drag and Drop",
    items: [
      event("drag-and-drop", "drag"),
      event("drag-and-drop", "dragstart"),
      event("drag-and-drop", "dragend"),
      event("drag-and-drop", "dragenter"),
      event("drag-and-drop", "dragover"),
      event("drag-and-drop", "dragleave"),
      event("drag-and-drop", "drop"),
    ],
  },
  {
    name: "Keyboard",
    items: [
      event("keyboard", "keydown"),
      event("keyboard", "keyup"),
      event("keyboard", "keypress"),
      event("keyboard", "input"),
    ],
  },
  {
    name: "Load",
    items: [
      event("load", "load", "global"),
      event("load", "beforeunload", "global"),
      event("load", "unload", "global"),
      event("load", "abort", "global"),
      event("load", "error", "global"),
      event("load", "hashchange", "global"),
      event("load", "popstate", "global"),
    ],
  },
  {
    name: "Media",
    items: [
      event("media", "play", "media"),
      event("media", "pause", "media"),
      event("media", "playing", "media"),
      event("media", "canplay", "media"),
      event("media", "canplaythrough", "media"),
      event("media", "seeking", "media"),
      event("media", "seeked", "media"),
      event("media", "timeupdate", "media"),
      event("media", "ended", "media"),
      event("media", "ratechange", "media"),
      event("media", "durationchange", "media"),
      event("media", "volumechange", "media"),
      event("media", "loadstart", "media"),
      event("media", "progress", "media"),
      event("media", "suspend", "media"),
      event("media", "abort", "media"),
      event("media", "error", "media"),
      event("media", "emptied", "media"),
      event("media", "stalled", "media"),
      event("media", "loadedmetadata", "media"),
      event("media", "loadeddata", "media"),
      event("media", "waiting", "media"),
    ],
  },
  {
    name: "Mouse",
    items: [
      event("mouse", "auxclick"),
      event("mouse", "click"),
      event("mouse", "dblclick"),
      event("mouse", "mousedown"),
      event("mouse", "mouseup"),
      event("mouse", "mouseover"),
      event("mouse", "mousemove"),
      event("mouse", "mouseout"),
      event("mouse", "mouseenter"),
      event("mouse", "mouseleave"),
      event("mouse", "mousewheel"),
      event("mouse", "wheel"),
      event("mouse", "contextmenu"),
    ],
  },
  {
    name: "Pointer",
    items: [
      event("pointer", "pointerover"),
      event("pointer", "pointerout"),
      event("pointer", "pointerenter"),
      event("pointer", "pointerleave"),
      event("pointer", "pointerdown"),
      event("pointer", "pointerup"),
      event("pointer", "pointermove"),
      event("pointer", "pointercancel"),
      event("pointer", "gotpointercapture"),
      event("pointer", "lostpointercapture"),
    ],
  },
  {
    name: "Touch",
    items: [
      event("touch", "touchstart"),
      event("touch", "touchmove"),
      event("touch", "touchend"),
      event("touch", "touchcancel"),
    ],
  },
  {
    name: "Worker",
    items: [
      event("worker", "message", "global"),
      event("worker", "messageerror", "global"),
    ],
  },
  {
    name: "XHR",
    items: [
      event("xhr", "readystatechange", "xhr"),
      event("xhr", "load", "xhr"),
      event("xhr", "loadstart", "xhr"),
      event("xhr", "loadend", "xhr"),
      event("xhr", "abort", "xhr"),
      event("xhr", "error", "xhr"),
      event("xhr", "progress", "xhr"),
      event("xhr", "timeout", "xhr"),
    ],
  },
];
