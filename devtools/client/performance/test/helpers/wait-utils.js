/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* globals dump */

const { CC } = require("chrome");
const { Task } = require("devtools/shared/task");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { once, observeOnce } = require("devtools/client/performance/test/helpers/event-utils");

/**
 * Blocks the main thread for the specified amount of time.
 */
exports.busyWait = function (time) {
  dump(`Busy waiting for: ${time} milliseconds.\n`);
  let start = Date.now();
  /* eslint-disable no-unused-vars */
  let stack;
  while (Date.now() - start < time) {
    stack = CC.stack;
  }
  /* eslint-enable no-unused-vars */
};

/**
 * Idly waits for the specified amount of time.
 */
exports.idleWait = function (time) {
  dump(`Idly waiting for: ${time} milliseconds.\n`);
  return DevToolsUtils.waitForTime(time);
};

/**
 * Waits until a predicate returns true.
 */
exports.waitUntil = function* (predicate, interval = 100, tries = 100) {
  for (let i = 1; i <= tries; i++) {
    if (yield Task.spawn(predicate)) {
      dump(`Predicate returned true after ${i} tries.\n`);
      return;
    }
    yield exports.idleWait(interval);
  }
  throw new Error(`Predicate returned false after ${tries} tries, aborting.\n`);
};

/**
 * Waits for a `MozAfterPaint` event to be fired on the specified window.
 */
exports.waitForMozAfterPaint = function (window) {
  return once(window, "MozAfterPaint");
};

/**
 * Waits for the `browser-delayed-startup-finished` observer notification
 * to be fired on the specified window.
 */
exports.waitForDelayedStartupFinished = function (window) {
  return observeOnce("browser-delayed-startup-finished", { expectedSubject: window });
};
