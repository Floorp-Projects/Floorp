/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

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
function on(element, type, listener, capture) {
  // `capture` defaults to `false`.
  capture = capture || false;
  element.addEventListener(type, listener, capture);
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
function once(element, type, listener, capture) {
  on(element, type, function selfRemovableListener(event) {
    removeListener(element, type, selfRemovableListener, capture);
    listener.apply(this, arguments);
  }, capture);
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
function removeListener(element, type, listener, capture) {
  element.removeEventListener(type, listener, capture);
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
function emit(element, type, { category, initializer, settings }) {
  category = category || "UIEvents";
  initializer = initializer || getInitializerName(category);
  let document = element.ownerDocument;
  let event = document.createEvent(category);
  event[initializer].apply(event, [type].concat(settings));
  element.dispatchEvent(event);
};
exports.emit = emit;
