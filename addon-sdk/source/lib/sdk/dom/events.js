/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cu } = require("chrome");
const { ShimWaiver } = Cu.import("resource://gre/modules/ShimWaiver.jsm");

// Utility function that returns copy of the given `text` with last character
// removed if it is `"s"`.
function singularify(text) {
  return text[text.length - 1] === "s" ? text.substr(0, text.length - 1) : text;
}

// Utility function that takes event type, argument is passed to
// `document.createEvent` and returns name of the initializer method of the
// given event. Please note that there are some event types whose initializer
// methods can't be guessed by this function. For more details see following
// link: https://developer.mozilla.org/En/DOM/Document.createEvent
function getInitializerName(category) {
  return "init" + singularify(category);
}

/**
 * Registers an event `listener` on a given `element`, that will be called
 * when events of specified `type` is dispatched on the `element`.
 * @param {Element} element
 *    Dom element to register listener on.
 * @param {String} type
 *    A string representing the
 *    [event type](https://developer.mozilla.org/en/DOM/event.type) to
 *    listen for.
 * @param {Function} listener
 *    Function that is called whenever an event of the specified `type`
 *    occurs.
 * @param {Boolean} capture
 *    If true, indicates that the user wishes to initiate capture. After
 *    initiating capture, all events of the specified type will be dispatched
 *    to the registered listener before being dispatched to any `EventTarget`s
 *    beneath it in the DOM tree. Events which are bubbling upward through
 *    the tree will not trigger a listener designated to use capture.
 *    See [DOM Level 3 Events](http://www.w3.org/TR/DOM-Level-3-Events/#event-flow)
 *    for a detailed explanation.
 */
function on(element, type, listener, capture, shimmed = false) {
  // `capture` defaults to `false`.
  capture = capture || false;
  if (shimmed) {
    element.addEventListener(type, listener, capture);
  } else {
    ShimWaiver.getProperty(element, "addEventListener")(type, listener, capture);
  }
}
exports.on = on;

/**
 * Registers an event `listener` on a given `element`, that will be called
 * only once, next time event of specified `type` is dispatched on the
 * `element`.
 * @param {Element} element
 *    Dom element to register listener on.
 * @param {String} type
 *    A string representing the
 *    [event type](https://developer.mozilla.org/en/DOM/event.type) to
 *    listen for.
 * @param {Function} listener
 *    Function that is called whenever an event of the specified `type`
 *    occurs.
 * @param {Boolean} capture
 *    If true, indicates that the user wishes to initiate capture. After
 *    initiating capture, all events of the specified type will be dispatched
 *    to the registered listener before being dispatched to any `EventTarget`s
 *    beneath it in the DOM tree. Events which are bubbling upward through
 *    the tree will not trigger a listener designated to use capture.
 *    See [DOM Level 3 Events](http://www.w3.org/TR/DOM-Level-3-Events/#event-flow)
 *    for a detailed explanation.
 */
function once(element, type, listener, capture, shimmed = false) {
  on(element, type, function selfRemovableListener(event) {
    removeListener(element, type, selfRemovableListener, capture, shimmed);
    listener.apply(this, arguments);
  }, capture, shimmed);
}
exports.once = once;

/**
 * Unregisters an event `listener` on a given `element` for the events of the
 * specified `type`.
 *
 * @param {Element} element
 *    Dom element to unregister listener from.
 * @param {String} type
 *    A string representing the
 *    [event type](https://developer.mozilla.org/en/DOM/event.type) to
 *    listen for.
 * @param {Function} listener
 *    Function that is called whenever an event of the specified `type`
 *    occurs.
 * @param {Boolean} capture
 *    If true, indicates that the user wishes to initiate capture. After
 *    initiating capture, all events of the specified type will be dispatched
 *    to the registered listener before being dispatched to any `EventTarget`s
 *    beneath it in the DOM tree. Events which are bubbling upward through
 *    the tree will not trigger a listener designated to use capture.
 *    See [DOM Level 3 Events](http://www.w3.org/TR/DOM-Level-3-Events/#event-flow)
 *    for a detailed explanation.
 */
function removeListener(element, type, listener, capture, shimmed = false) {
  if (shimmed) {
    element.removeEventListener(type, listener, capture);
  } else {
    ShimWaiver.getProperty(element, "removeEventListener")(type, listener, capture);
  }
}
exports.removeListener = removeListener;

/**
 * Emits event of the specified `type` and `category` on the given `element`.
 * Specified `settings` are used to initialize event before dispatching it.
 * @param {Element} element
 *    Dom element to dispatch event on.
 * @param {String} type
 *    A string representing the
 *    [event type](https://developer.mozilla.org/en/DOM/event.type).
 * @param {Object} options
 *    Options object containing following properties:
 *    - `category`: String passed to the `document.createEvent`. Option is
 *      optional and defaults to "UIEvents".
 *    - `initializer`: If passed it will be used as name of the method used
 *      to initialize event. If omitted name will be generated from the
 *      `category` field by prefixing it with `"init"` and removing last
 *      character if it matches `"s"`.
 *    - `settings`: Array of settings that are forwarded to the event
 *      initializer after firs `type` argument.
 * @see https://developer.mozilla.org/En/DOM/Document.createEvent
 */
function emit(element, type, { category, initializer, settings }, shimmed = false) {
  category = category || "UIEvents";
  initializer = initializer || getInitializerName(category);
  let document = element.ownerDocument;
  let event = document.createEvent(category);
  event[initializer].apply(event, [type].concat(settings));
  if (shimmed) {
    element.dispatchEvent(event);
  } else {
    ShimWaiver.getProperty(element, "dispatchEvent")(event);
  }
};
exports.emit = emit;

// Takes DOM `element` and returns promise which is resolved
// when given element is removed from it's parent node.
const removed = element => {
  return new Promise(resolve => {
    const { MutationObserver } = element.ownerDocument.defaultView;
    const observer = new MutationObserver(mutations => {
      for (let mutation of mutations) {
        for (let node of mutation.removedNodes || []) {
          if (node === element) {
            observer.disconnect();
            resolve(element);
          }
        }
      }
    });
    observer.observe(element.parentNode, {childList: true});
  });
};
exports.removed = removed;

const when = (element, eventName, capture=false, shimmed=false) => new Promise(resolve => {
  const listener = event => {
    if (shimmed) {
      element.removeEventListener(eventName, listener, capture);
    } else {
      ShimWaiver.getProperty(element, "removeEventListener")(eventName, listener, capture);
    }
    resolve(event);
  };

  if (shimmed) {
    element.addEventListener(eventName, listener, capture);
  } else {
    ShimWaiver.getProperty(element, "addEventListener")(eventName, listener, capture);
  }
});
exports.when = when;
