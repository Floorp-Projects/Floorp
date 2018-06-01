/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* globals dump */

const Services = require("Services");

const KNOWN_EE_APIS = [
  ["on", "off"],
  ["addEventListener", "removeEventListener"],
  ["addListener", "removeListener"]
];

/**
 * Listens for any event for a single time on a target, no matter what kind of
 * event emitter it is, returning a promise resolved with the passed arguments
 * once the event is fired.
 */
exports.once = function(target, eventName, options = {}) {
  return exports.times(target, eventName, 1, options);
};

/**
 * Waits for any event to be fired a specified amount of times on a target, no
 * matter what kind of event emitter.
 * Possible options: `useCapture`, `spreadArgs`, `expectedArgs`
 */
exports.times = function(target, eventName, receiveCount, options = {}) {
  const msg =
    `Waiting for event: '${eventName}' on ${target} for ${receiveCount} time(s)`;
  if ("expectedArgs" in options) {
    dump(`${msg} with arguments: ${JSON.stringify(options.expectedArgs)}.\n`);
  } else {
    dump(`${msg}.\n`);
  }

  return new Promise((resolve, reject) => {
    if (typeof eventName != "string") {
      reject(new Error(`Unexpected event name: ${eventName}.`));
    }

    const API = KNOWN_EE_APIS.find(([a, r]) => (a in target) && (r in target));
    if (!API) {
      reject(new Error("Target is not a supported event listener."));
      return;
    }

    const [add, remove] = API;

    target[add](eventName, function onEvent(...args) {
      if ("expectedArgs" in options) {
        for (const [index, expectedValue] of options.expectedArgs.entries()) {
          const isExpectedValueRegExp = expectedValue instanceof RegExp;
          if (
            (isExpectedValueRegExp && !expectedValue.exec(args[index])) ||
            (!isExpectedValueRegExp && expectedValue != args[index])
          ) {
            dump(`Ignoring event '${eventName}' with unexpected argument at index ` +
                 `${index}: ${args[index]} - expected ${expectedValue}\n`);
            return;
          }
        }
      }
      if (--receiveCount > 0) {
        dump(`Event: '${eventName}' on ${target} needs to be fired ${receiveCount} ` +
             `more time(s).\n`);
      } else if (!receiveCount) {
        dump(`Event: '${eventName}' on ${target} received.\n`);
        target[remove](eventName, onEvent, options.useCapture);
        resolve(options.spreadArgs ? args : args[0]);
      }
    }, options.useCapture);
  });
};

/**
 * Like `once`, but for observer notifications.
 */
exports.observeOnce = function(notificationName, options = {}) {
  return exports.observeTimes(notificationName, 1, options);
};

/**
 * Like `times`, but for observer notifications.
 * Possible options: `expectedSubject`
 */
exports.observeTimes = function(notificationName, receiveCount, options = {}) {
  dump(`Waiting for notification: '${notificationName}' for ${receiveCount} time(s).\n`);

  return new Promise((resolve, reject) => {
    if (typeof notificationName != "string") {
      reject(new Error(`Unexpected notification name: ${notificationName}.`));
    }

    Services.obs.addObserver(function onObserve(subject, topic, data) {
      if ("expectedSubject" in options && options.expectedSubject != subject) {
        dump(`Ignoring notification '${notificationName}' with unexpected subject: ` +
             `${subject}\n`);
        return;
      }
      if (--receiveCount > 0) {
        dump(`Notification: '${notificationName}' needs to be fired ${receiveCount} ` +
             `more time(s).\n`);
      } else if (!receiveCount) {
        dump(`Notification: '${notificationName}' received.\n`);
        Services.obs.removeObserver(onObserve, topic);
        resolve(data);
      }
    }, notificationName);
  });
};
