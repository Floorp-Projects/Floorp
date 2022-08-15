/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function generalEvent(groupID, eventType) {
  return {
    id: `event.${groupID}.${eventType}`,
    type: "event",
    name: eventType,
    message: `DOM '${eventType}' event`,
    eventType,
    filter: "general",
  };
}
function nodeEvent(groupID, eventType) {
  return {
    ...generalEvent(groupID, eventType),
    filter: "node",
  };
}
function mediaNodeEvent(groupID, eventType) {
  return {
    ...generalEvent(groupID, eventType),
    filter: "media",
  };
}
function globalEvent(groupID, eventType) {
  return {
    ...generalEvent(groupID, eventType),
    message: `Global '${eventType}' event`,
    filter: "global",
  };
}
function xhrEvent(groupID, eventType) {
  return {
    ...generalEvent(groupID, eventType),
    message: `XHR '${eventType}' event`,
    filter: "xhr",
  };
}

function webSocketEvent(groupID, eventType) {
  return {
    ...generalEvent(groupID, eventType),
    message: `WebSocket '${eventType}' event`,
    filter: "websocket",
  };
}

function workerEvent(eventType) {
  return {
    ...generalEvent("worker", eventType),
    message: `Worker '${eventType}' event`,
    filter: "worker",
  };
}

function timerEvent(type, operation, name, notificationType) {
  return {
    id: `timer.${type}.${operation}`,
    type: "simple",
    name,
    message: name,
    notificationType,
  };
}

function animationEvent(operation, name, notificationType) {
  return {
    id: `animationframe.${operation}`,
    type: "simple",
    name,
    message: name,
    notificationType,
  };
}

const SCRIPT_FIRST_STATEMENT_BREAKPOINT = {
  id: "script.source.firstStatement",
  type: "script",
  name: "Script First Statement",
  message: "Script First Statement",
};

const AVAILABLE_BREAKPOINTS = [
  {
    name: "Animation",
    items: [
      animationEvent(
        "request",
        "Request Animation Frame",
        "requestAnimationFrame"
      ),
      animationEvent(
        "cancel",
        "Cancel Animation Frame",
        "cancelAnimationFrame"
      ),
      animationEvent(
        "fire",
        "Animation Frame fired",
        "requestAnimationFrameCallback"
      ),
    ],
  },
  {
    name: "Clipboard",
    items: [
      generalEvent("clipboard", "copy"),
      generalEvent("clipboard", "cut"),
      generalEvent("clipboard", "paste"),
      generalEvent("clipboard", "beforecopy"),
      generalEvent("clipboard", "beforecut"),
      generalEvent("clipboard", "beforepaste"),
    ],
  },
  {
    name: "Control",
    items: [
      generalEvent("control", "resize"),
      generalEvent("control", "scroll"),
      generalEvent("control", "zoom"),
      generalEvent("control", "focus"),
      generalEvent("control", "focusin"),
      generalEvent("control", "focusout"),
      generalEvent("control", "blur"),
      generalEvent("control", "select"),
      generalEvent("control", "change"),
      generalEvent("control", "submit"),
      generalEvent("control", "reset"),
    ],
  },
  {
    name: "DOM Mutation",
    items: [
      // Deprecated DOM events.
      nodeEvent("dom-mutation", "DOMActivate"),
      nodeEvent("dom-mutation", "DOMFocusIn"),
      nodeEvent("dom-mutation", "DOMFocusOut"),

      // Standard DOM mutation events.
      nodeEvent("dom-mutation", "DOMAttrModified"),
      nodeEvent("dom-mutation", "DOMCharacterDataModified"),
      nodeEvent("dom-mutation", "DOMNodeInserted"),
      nodeEvent("dom-mutation", "DOMNodeInsertedIntoDocument"),
      nodeEvent("dom-mutation", "DOMNodeRemoved"),
      nodeEvent("dom-mutation", "DOMNodeRemovedIntoDocument"),
      nodeEvent("dom-mutation", "DOMSubtreeModified"),

      // DOM load events.
      nodeEvent("dom-mutation", "DOMContentLoaded"),
    ],
  },
  {
    name: "Device",
    items: [
      globalEvent("device", "deviceorientation"),
      globalEvent("device", "devicemotion"),
    ],
  },
  {
    name: "Drag and Drop",
    items: [
      generalEvent("drag-and-drop", "drag"),
      generalEvent("drag-and-drop", "dragstart"),
      generalEvent("drag-and-drop", "dragend"),
      generalEvent("drag-and-drop", "dragenter"),
      generalEvent("drag-and-drop", "dragover"),
      generalEvent("drag-and-drop", "dragleave"),
      generalEvent("drag-and-drop", "drop"),
    ],
  },
  {
    name: "Keyboard",
    items: [
      generalEvent("keyboard", "beforeinput"),
      generalEvent("keyboard", "input"),
      generalEvent("keyboard", "keydown"),
      generalEvent("keyboard", "keyup"),
      generalEvent("keyboard", "keypress"),
      generalEvent("keyboard", "compositionstart"),
      generalEvent("keyboard", "compositionupdate"),
      generalEvent("keyboard", "compositionend"),
    ].filter(Boolean),
  },
  {
    name: "Load",
    items: [
      globalEvent("load", "load"),
      // TODO: Disabled pending fixes for bug 1569775.
      // globalEvent("load", "beforeunload"),
      // globalEvent("load", "unload"),
      globalEvent("load", "abort"),
      globalEvent("load", "error"),
      globalEvent("load", "hashchange"),
      globalEvent("load", "popstate"),
    ],
  },
  {
    name: "Media",
    items: [
      mediaNodeEvent("media", "play"),
      mediaNodeEvent("media", "pause"),
      mediaNodeEvent("media", "playing"),
      mediaNodeEvent("media", "canplay"),
      mediaNodeEvent("media", "canplaythrough"),
      mediaNodeEvent("media", "seeking"),
      mediaNodeEvent("media", "seeked"),
      mediaNodeEvent("media", "timeupdate"),
      mediaNodeEvent("media", "ended"),
      mediaNodeEvent("media", "ratechange"),
      mediaNodeEvent("media", "durationchange"),
      mediaNodeEvent("media", "volumechange"),
      mediaNodeEvent("media", "loadstart"),
      mediaNodeEvent("media", "progress"),
      mediaNodeEvent("media", "suspend"),
      mediaNodeEvent("media", "abort"),
      mediaNodeEvent("media", "error"),
      mediaNodeEvent("media", "emptied"),
      mediaNodeEvent("media", "stalled"),
      mediaNodeEvent("media", "loadedmetadata"),
      mediaNodeEvent("media", "loadeddata"),
      mediaNodeEvent("media", "waiting"),
    ],
  },
  {
    name: "Mouse",
    items: [
      generalEvent("mouse", "auxclick"),
      generalEvent("mouse", "click"),
      generalEvent("mouse", "dblclick"),
      generalEvent("mouse", "mousedown"),
      generalEvent("mouse", "mouseup"),
      generalEvent("mouse", "mouseover"),
      generalEvent("mouse", "mousemove"),
      generalEvent("mouse", "mouseout"),
      generalEvent("mouse", "mouseenter"),
      generalEvent("mouse", "mouseleave"),
      generalEvent("mouse", "mousewheel"),
      generalEvent("mouse", "wheel"),
      generalEvent("mouse", "contextmenu"),
    ],
  },
  {
    name: "Pointer",
    items: [
      generalEvent("pointer", "pointerover"),
      generalEvent("pointer", "pointerout"),
      generalEvent("pointer", "pointerenter"),
      generalEvent("pointer", "pointerleave"),
      generalEvent("pointer", "pointerdown"),
      generalEvent("pointer", "pointerup"),
      generalEvent("pointer", "pointermove"),
      generalEvent("pointer", "pointercancel"),
      generalEvent("pointer", "gotpointercapture"),
      generalEvent("pointer", "lostpointercapture"),
    ],
  },
  {
    name: "Script",
    items: [SCRIPT_FIRST_STATEMENT_BREAKPOINT],
  },
  {
    name: "Timer",
    items: [
      timerEvent("timeout", "set", "setTimeout", "setTimeout"),
      timerEvent("timeout", "clear", "clearTimeout", "clearTimeout"),
      timerEvent("timeout", "fire", "setTimeout fired", "setTimeoutCallback"),
      timerEvent("interval", "set", "setInterval", "setInterval"),
      timerEvent("interval", "clear", "clearInterval", "clearInterval"),
      timerEvent(
        "interval",
        "fire",
        "setInterval fired",
        "setIntervalCallback"
      ),
    ],
  },
  {
    name: "Touch",
    items: [
      generalEvent("touch", "touchstart"),
      generalEvent("touch", "touchmove"),
      generalEvent("touch", "touchend"),
      generalEvent("touch", "touchcancel"),
    ],
  },
  {
    name: "WebSocket",
    items: [
      webSocketEvent("websocket", "open"),
      webSocketEvent("websocket", "message"),
      webSocketEvent("websocket", "error"),
      webSocketEvent("websocket", "close"),
    ],
  },
  {
    name: "Worker",
    items: [
      workerEvent("message"),
      workerEvent("messageerror"),

      // Service Worker events.
      globalEvent("serviceworker", "fetch"),
    ],
  },
  {
    name: "XHR",
    items: [
      xhrEvent("xhr", "readystatechange"),
      xhrEvent("xhr", "load"),
      xhrEvent("xhr", "loadstart"),
      xhrEvent("xhr", "loadend"),
      xhrEvent("xhr", "abort"),
      xhrEvent("xhr", "error"),
      xhrEvent("xhr", "progress"),
      xhrEvent("xhr", "timeout"),
    ],
  },
];

const FLAT_EVENTS = [];
for (const category of AVAILABLE_BREAKPOINTS) {
  for (const event of category.items) {
    FLAT_EVENTS.push(event);
  }
}
const EVENTS_BY_ID = {};
for (const event of FLAT_EVENTS) {
  if (EVENTS_BY_ID[event.id]) {
    throw new Error("Duplicate event ID detected: " + event.id);
  }
  EVENTS_BY_ID[event.id] = event;
}

const SIMPLE_EVENTS = {};
const DOM_EVENTS = {};
for (const eventBP of FLAT_EVENTS) {
  if (eventBP.type === "simple") {
    const { notificationType } = eventBP;
    if (SIMPLE_EVENTS[notificationType]) {
      throw new Error("Duplicate simple event");
    }
    SIMPLE_EVENTS[notificationType] = eventBP.id;
  } else if (eventBP.type === "event") {
    const { eventType, filter } = eventBP;

    let targetTypes;
    if (filter === "global") {
      targetTypes = ["global"];
    } else if (filter === "xhr") {
      targetTypes = ["xhr"];
    } else if (filter === "websocket") {
      targetTypes = ["websocket"];
    } else if (filter === "worker") {
      targetTypes = ["worker"];
    } else if (filter === "general") {
      targetTypes = ["global", "node"];
    } else if (filter === "node" || filter === "media") {
      targetTypes = ["node"];
    } else {
      throw new Error("Unexpected filter type");
    }

    for (const targetType of targetTypes) {
      let byEventType = DOM_EVENTS[targetType];
      if (!byEventType) {
        byEventType = {};
        DOM_EVENTS[targetType] = byEventType;
      }

      if (byEventType[eventType]) {
        throw new Error("Duplicate dom event: " + eventType);
      }
      byEventType[eventType] = eventBP.id;
    }
  } else if (eventBP.type === "script") {
    // Nothing to do.
  } else {
    throw new Error("Unknown type: " + eventBP.type);
  }
}

exports.eventBreakpointForNotification = eventBreakpointForNotification;
function eventBreakpointForNotification(dbg, notification) {
  const notificationType = notification.type;

  if (notification.type === "domEvent") {
    const domEventNotification = DOM_EVENTS[notification.targetType];
    if (!domEventNotification) {
      return null;
    }

    // The 'event' value is a cross-compartment wrapper for the DOM Event object.
    // While we could use that directly in the main thread as an Xray wrapper,
    // when debugging workers we can't, because it is an opaque wrapper.
    // To make things work, we have to always interact with the Event object via
    // the Debugger.Object interface.
    const evt = dbg
      .makeGlobalObjectReference(notification.global)
      .makeDebuggeeValue(notification.event);

    const eventType = evt.getProperty("type").return;
    const id = domEventNotification[eventType];
    if (!id) {
      return null;
    }
    const eventBreakpoint = EVENTS_BY_ID[id];

    if (eventBreakpoint.filter === "media") {
      const currentTarget = evt.getProperty("currentTarget").return;
      if (!currentTarget) {
        return null;
      }

      const nodeType = currentTarget.getProperty("nodeType").return;
      const namespaceURI = currentTarget.getProperty("namespaceURI").return;
      if (
        nodeType !== 1 /* ELEMENT_NODE */ ||
        namespaceURI !== "http://www.w3.org/1999/xhtml"
      ) {
        return null;
      }

      const nodeName = currentTarget
        .getProperty("nodeName")
        .return.toLowerCase();
      if (nodeName !== "audio" && nodeName !== "video") {
        return null;
      }
    }

    return id;
  }

  return SIMPLE_EVENTS[notificationType] || null;
}

exports.makeEventBreakpointMessage = makeEventBreakpointMessage;
function makeEventBreakpointMessage(id) {
  return EVENTS_BY_ID[id].message;
}

exports.firstStatementBreakpointId = firstStatementBreakpointId;
function firstStatementBreakpointId() {
  return SCRIPT_FIRST_STATEMENT_BREAKPOINT.id;
}

exports.eventsRequireNotifications = eventsRequireNotifications;
function eventsRequireNotifications(ids) {
  for (const id of ids) {
    const eventBreakpoint = EVENTS_BY_ID[id];

    // Script events are implemented directly in the server and do not require
    // notifications from Gecko, so there is no need to watch for them.
    if (eventBreakpoint && eventBreakpoint.type !== "script") {
      return true;
    }
  }
  return false;
}

exports.getAvailableEventBreakpoints = getAvailableEventBreakpoints;
function getAvailableEventBreakpoints() {
  const available = [];
  for (const { name, items } of AVAILABLE_BREAKPOINTS) {
    available.push({
      name,
      events: items.map(item => ({
        id: item.id,
        name: item.name,
      })),
    });
  }
  return available;
}
exports.validateEventBreakpoint = validateEventBreakpoint;
function validateEventBreakpoint(id) {
  return !!EVENTS_BY_ID[id];
}
