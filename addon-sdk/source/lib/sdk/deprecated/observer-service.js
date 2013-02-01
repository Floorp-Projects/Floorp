/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "deprecated"
};

const { Cc, Ci } = require("chrome");
const { when: unload } = require("../system/unload");
const { ns } = require("../core/namespace");
const { on, off, emit, once } = require("../system/events");
const { id } = require("../self");

const subscribers = ns();
const cache = [];

/**
 * Topics specifically available to Jetpack-generated extensions.
 *
 * Using these predefined consts instead of the platform strings is good:
 *   - allows us to scope topics specifically for Jetpacks
 *   - addons aren't dependent on strings nor behavior of core platform topics
 *   - the core platform topics are not clearly named
 *
 */
exports.topics = {
  /**
   * A topic indicating that the application is in a state usable
   * by add-ons.
   */
  APPLICATION_READY: id + "_APPLICATION_READY"
};

function Listener(callback, target) {
  return function listener({ subject, data }) {
    callback.call(target || callback, subject, data);
  }
}

/**
 * Register the given callback as an observer of the given topic.
 *
 * @param   topic       {String}
 *          the topic to observe
 *
 * @param   callback    {Object}
 *          the callback; an Object that implements nsIObserver or a Function
 *          that gets called when the notification occurs
 *
 * @param   target  {Object}  [optional]
 *          the object to use as |this| when calling a Function callback
 *
 * @returns the observer
 */
function add(topic, callback, target) {
  let listeners = subscribers(callback);
  if (!(topic in listeners)) {
    let listener = Listener(callback, target);
    listeners[topic] = listener;

    // Cache callback unless it's already cached.
    if (!~cache.indexOf(callback))
      cache.push(callback);

    on(topic, listener);
  }
};
exports.add = add;

/**
 * Unregister the given callback as an observer of the given topic.
 *
 * @param   topic       {String}
 *          the topic being observed
 *
 * @param   callback    {Object}
 *          the callback doing the observing
 *
 * @param   target  {Object}  [optional]
 *          the object being used as |this| when calling a Function callback
 */
function remove(topic, callback, target) {
  let listeners = subscribers(callback);
  if (topic in listeners) {
    let listener = listeners[topic];
    delete listeners[topic];

    // If no more observers are registered and callback is still in cache
    // then remove it.
    let index = cache.indexOf(callback);
    if (~index && !Object.keys(listeners).length)
      cache.splice(index, 1)

    off(topic, listener);
  }
};
exports.remove = remove;

/**
 * Notify observers about something.
 *
 * @param topic   {String}
 *        the topic to notify observers about
 *
 * @param subject {Object}  [optional]
 *        some information about the topic; can be any JS object or primitive
 *
 * @param data    {String}  [optional] [deprecated]
 *        some more information about the topic; deprecated as the subject
 *        is sufficient to pass all needed information to the JS observers
 *        that this module targets; if you have multiple values to pass to
 *        the observer, wrap them in an object and pass them via the subject
 *        parameter (i.e.: { foo: 1, bar: "some string", baz: myObject })
 */
function notify(topic, subject, data) {
  emit(topic, {
    subject: subject === undefined ? null : subject,
    data: data === undefined ? null : data
  });
}
exports.notify = notify;

unload(function() {
  // Make a copy of cache first, since cache will be changing as we
  // iterate through it.
  cache.slice().forEach(function(callback) {
    Object.keys(subscribers(callback)).forEach(function(topic) {
      remove(topic, callback);
    });
  });
})
