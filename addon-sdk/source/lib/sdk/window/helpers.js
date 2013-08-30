/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { defer } = require('../core/promise');
const events = require('../system/events');
const { open: openWindow, onFocus, getToplevelWindow,
        isInteractive } = require('./utils');

function open(uri, options) {
  return promise(openWindow.apply(null, arguments), 'load');
}
exports.open = open;

function close(window) {
  // We shouldn't wait for unload, as it is dispatched
  // before the window is actually closed.
  // `domwindowclosed` is a better match.
  let deferred = defer();
  let toplevelWindow = getToplevelWindow(window);
  events.on("domwindowclosed", function onclose({subject}) {
    if (subject == toplevelWindow) {
      events.off("domwindowclosed", onclose);
      deferred.resolve(window);
    }
  }, true);
  window.close();
  return deferred.promise;
}
exports.close = close;

function focus(window) {
  let p = onFocus(window);
  window.focus();
  return p;
}
exports.focus = focus;

function ready(window) {
  let { promise: result, resolve } = defer();

  if (isInteractive(window))
    resolve(window);
  else
    resolve(promise(window, 'DOMContentLoaded'));

  return result;
}
exports.ready = ready;

function promise(target, evt, capture) {
  let deferred = defer();
  capture = !!capture;

  target.addEventListener(evt, function eventHandler() {
    target.removeEventListener(evt, eventHandler, capture);
    deferred.resolve(target);
  }, capture);

  return deferred.promise;
}
exports.promise = promise;
