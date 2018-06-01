/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file contains event parsers that are then used by developer tools in
// order to find information about events affecting an HTML element.

"use strict";

const Services = require("Services");

// eslint-disable-next-line
const JQUERY_LIVE_REGEX = /return typeof \w+.*.event\.triggered[\s\S]*\.event\.(dispatch|handle).*arguments/;

var parsers = [
  {
    id: "jQuery events",
    getListeners: function(node) {
      const global = node.ownerGlobal.wrappedJSObject;
      const hasJQuery = global.jQuery && global.jQuery.fn && global.jQuery.fn.jquery;

      if (!hasJQuery) {
        return undefined;
      }

      const jQuery = global.jQuery;
      const handlers = [];

      // jQuery 1.2+
      const data = jQuery._data || jQuery.data;
      if (data) {
        const eventsObj = data(node, "events");
        for (const type in eventsObj) {
          const events = eventsObj[type];
          for (const key in events) {
            const event = events[key];

            if (node.wrappedJSObject == global.document && event.selector) {
              continue;
            }

            if (typeof event === "object" || typeof event === "function") {
              const eventInfo = {
                type: type,
                handler: event.handler || event,
                tags: "jQuery",
                hide: {
                  capturing: true,
                  dom0: true
                }
              };

              handlers.push(eventInfo);
            }
          }
        }
      }

      // JQuery 1.0 & 1.1
      const entry = jQuery(node)[0];

      if (!entry) {
        return handlers;
      }

      for (const type in entry.events) {
        const events = entry.events[type];
        for (const key in events) {
          const event = events[key];

          if (node.wrappedJSObject == global.document && event.selector) {
            continue;
          }

          if (typeof events[key] === "function") {
            const eventInfo = {
              type: type,
              handler: events[key],
              tags: "jQuery",
              hide: {
                capturing: true,
                dom0: true
              }
            };

            handlers.push(eventInfo);
          }
        }
      }

      return handlers;
    }
  },
  {
    id: "jQuery live events",
    hasListeners: function(node) {
      return jQueryLiveGetListeners(node, true);
    },
    getListeners: function(node) {
      return jQueryLiveGetListeners(node, false);
    },
    normalizeListener: function(handlerDO) {
      function isFunctionInProxy(funcDO) {
        // If the anonymous function is inside the |proxy| function and the
        // function only has guessed atom, the guessed atom should starts with
        // "proxy/".
        const displayName = funcDO.displayName;
        if (displayName && displayName.startsWith("proxy/")) {
          return true;
        }

        // If the anonymous function is inside the |proxy| function and the
        // function gets name at compile time by SetFunctionName, its guessed
        // atom doesn't contain "proxy/".  In that case, check if the caller is
        // "proxy" function, as a fallback.
        const calleeDO = funcDO.environment.callee;
        if (!calleeDO) {
          return false;
        }
        const calleeName = calleeDO.displayName;
        return calleeName == "proxy";
      }

      function getFirstFunctionVariable(funcDO) {
        // The handler function inside the |proxy| function should point the
        // unwrapped function via environment variable.
        const names = funcDO.environment.names();
        for (const varName of names) {
          const varDO = handlerDO.environment.getVariable(varName);
          if (!varDO) {
            continue;
          }
          if (varDO.class == "Function") {
            return varDO;
          }
        }
        return null;
      }

      if (!isFunctionInProxy(handlerDO)) {
        return handlerDO;
      }

      const MAX_NESTED_HANDLER_COUNT = 2;
      for (let i = 0; i < MAX_NESTED_HANDLER_COUNT; i++) {
        const funcDO = getFirstFunctionVariable(handlerDO);
        if (!funcDO) {
          return handlerDO;
        }

        handlerDO = funcDO;
        if (isFunctionInProxy(handlerDO)) {
          continue;
        }
        break;
      }

      return handlerDO;
    }
  },
  {
    id: "DOM events",
    hasListeners: function(node) {
      let listeners;

      if (node.nodeName.toLowerCase() === "html") {
        const winListeners =
          Services.els.getListenerInfoFor(node.ownerGlobal) || [];
        const docElementListeners =
          Services.els.getListenerInfoFor(node) || [];
        const docListeners =
          Services.els.getListenerInfoFor(node.parentNode) || [];

        listeners = [...winListeners, ...docElementListeners, ...docListeners];
      } else {
        listeners = Services.els.getListenerInfoFor(node) || [];
      }

      for (const listener of listeners) {
        if (listener.listenerObject && listener.type) {
          return true;
        }
      }

      return false;
    },
    getListeners: function(node) {
      const handlers = [];
      const listeners = Services.els.getListenerInfoFor(node);

      // The Node actor's getEventListenerInfo knows that when an html tag has
      // been passed we need the window object so we don't need to account for
      // event hoisting here as we did in hasListeners.

      for (const listenerObj of listeners) {
        const listener = listenerObj.listenerObject;

        // If there is no JS event listener skip this.
        if (!listener || JQUERY_LIVE_REGEX.test(listener.toString())) {
          continue;
        }

        const eventInfo = {
          capturing: listenerObj.capturing,
          type: listenerObj.type,
          handler: listener
        };

        handlers.push(eventInfo);
      }

      return handlers;
    }
  },

  {
    id: "React events",
    hasListeners: function(node) {
      return reactGetListeners(node, true);
    },

    getListeners: function(node) {
      return reactGetListeners(node, false);
    },

    normalizeListener: function(handlerDO, listener) {
      let functionText = "";

      if (handlerDO.boundTargetFunction) {
        handlerDO = handlerDO.boundTargetFunction;
      }

      const introScript = handlerDO.script.source.introductionScript;
      const script = handlerDO.script;

      // If this is a Babel transpiled function we have no access to the
      // source location so we need to hide the filename and debugger
      // icon.
      if (introScript && introScript.displayName.endsWith("/transform.run")) {
        listener.hide.debugger = true;
        listener.hide.filename = true;

        if (!handlerDO.isArrowFunction) {
          functionText += "function (";
        } else {
          functionText += "(";
        }

        functionText += handlerDO.parameterNames.join(", ");

        functionText += ") {\n";

        const scriptSource = script.source.text;
        functionText +=
          scriptSource.substr(script.sourceStart, script.sourceLength);

        listener.override.handler = functionText;
      }

      return handlerDO;
    }
  },
];

function reactGetListeners(node, boolOnEventFound) {
  function getProps() {
    for (const key of Object.keys(node)) {
      if (key.startsWith("__reactInternalInstance$")) {
        const value = node[key];
        if (value.memoizedProps) {
          return value.memoizedProps; // React 16
        }
        return value._currentElement.props; // React 15
      }
    }
    return null;
  }

  node = node.wrappedJSObject || node;

  const handlers = [];
  const props = getProps();

  if (props) {
    for (const name in props) {
      if (name.startsWith("on")) {
        const prop = props[name];
        const listener = prop.__reactBoundMethod || prop;

        if (typeof listener !== "function") {
          continue;
        }

        if (boolOnEventFound) {
          return true;
        }

        const handler = {
          type: name,
          handler: listener,
          tags: "React",
          hide: {
            dom0: true
          },
          override: {
            capturing: name.endsWith("Capture")
          }
        };

        handlers.push(handler);
      }
    }
  }

  if (boolOnEventFound) {
    return false;
  }

  return handlers;
}

function jQueryLiveGetListeners(node, boolOnEventFound) {
  const global = node.ownerGlobal.wrappedJSObject;
  const hasJQuery = global.jQuery && global.jQuery.fn && global.jQuery.fn.jquery;

  if (!hasJQuery) {
    return undefined;
  }

  const jQuery = global.jQuery;
  const handlers = [];
  const data = jQuery._data || jQuery.data;

  if (data) {
    // Live events are added to the document and bubble up to all elements.
    // Any element matching the specified selector will trigger the live
    // event.
    const events = data(global.document, "events");

    for (const type in events) {
      const eventHolder = events[type];

      for (const idx in eventHolder) {
        if (typeof idx !== "string" || isNaN(parseInt(idx, 10))) {
          continue;
        }

        const event = eventHolder[idx];
        let selector = event.selector;

        if (!selector && event.data) {
          selector = event.data.selector || event.data || event.selector;
        }

        if (!selector || !node.ownerDocument) {
          continue;
        }

        let matches;
        try {
          matches = node.matches && node.matches(selector);
        } catch (e) {
          // Invalid selector, do nothing.
        }

        if (boolOnEventFound && matches) {
          return true;
        }

        if (!matches) {
          continue;
        }

        if (!boolOnEventFound &&
            (typeof event === "object" || typeof event === "function")) {
          const eventInfo = {
            type: event.origType || event.type.substr(selector.length + 1),
            handler: event.handler || event,
            tags: "jQuery,Live",
            hide: {
              dom0: true,
              capturing: true
            }
          };

          if (!eventInfo.type && event.data && event.data.live) {
            eventInfo.type = event.data.live;
          }

          handlers.push(eventInfo);
        }
      }
    }
  }

  if (boolOnEventFound) {
    return false;
  }
  return handlers;
}

this.EventParsers = function EventParsers() {
  if (this._eventParsers.size === 0) {
    for (const parserObj of parsers) {
      this.registerEventParser(parserObj);
    }
  }
};

exports.EventParsers = EventParsers;

EventParsers.prototype = {
  // NOTE: This is shared amongst all instances.
  _eventParsers: new Map(),

  get parsers() {
    return this._eventParsers;
  },

  /**
   * Register a new event parser to be used in the processing of event info.
   *
   * @param {Object} parserObj
   *        Each parser must contain the following properties:
   *        - parser, which must take the following form:
   *   {
   *     id {String}: "jQuery events",          // Unique id.
   *     getListeners: function(node) { },      // Function that takes a node
   *                                            // and returns an array of
   *                                            // eventInfo objects (see
   *                                            // below).
   *
   *     hasListeners: function(node) { },      // Optional function that takes
   *                                            // a node and returns a boolean
   *                                            // indicating whether a node has
   *                                            // listeners attached.
   *
   *     normalizeListener:
   *        function(fnDO, listener) { },       // Optional function that takes
   *                                            // a Debugger.Object instance
   *                                            // and an optional listener
   *                                            // object. Both objects can be
   *                                            // manipulated to display the
   *                                            // correct information in the
   *                                            // event bubble.
   *   }
   *
   * An eventInfo object should take the following form:
   *   {
   *     type {String}:      "click",
   *     handler {Function}: event handler,
   *     tags {String}:      "jQuery,Live", // These tags will be displayed as
   *                                        // attributes in the events popup.
   *     hide: {               // Hide or show fields:
   *       debugger: false,    // Debugger icon
   *       type: false,        // Event type e.g. click
   *       filename: false,    // Filename
   *       capturing: false,   // Capturing
   *       dom0: false         // DOM 0
   *     },
   *
   *     override: {                        // The following can be overridden:
   *       handler: someEventHandler,
   *       type: "click",
   *       origin: "http://www.mozilla.com",
   *       searchString: 'onclick="doSomething()"',
   *       dom0: true,
   *       capturing: true
   *     }
   *   }
   */
  registerEventParser: function(parserObj) {
    const parserId = parserObj.id;

    if (!parserId) {
      throw new Error("Cannot register new event parser with id " + parserId);
    }
    if (this._eventParsers.has(parserId)) {
      throw new Error("Duplicate event parser id " + parserId);
    }

    this._eventParsers.set(parserId, {
      getListeners: parserObj.getListeners,
      hasListeners: parserObj.hasListeners,
      normalizeListener: parserObj.normalizeListener
    });
  },

  /**
   * Removes parser that matches a given parserId.
   *
   * @param {String} parserId
   *        id of the event parser to unregister.
   */
  unregisterEventParser: function(parserId) {
    this._eventParsers.delete(parserId);
  },

  /**
   * Tidy up parsers.
   */
  destroy: function() {
    for (const [id] of this._eventParsers) {
      this.unregisterEventParser(id, true);
    }
  }
};
