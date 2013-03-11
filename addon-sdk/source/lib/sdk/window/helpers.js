/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { defer } = require('../core/promise');
const { open: openWindow, onFocus } = require('./utils');

function open(uri, options) {
  return promise(openWindow.apply(null, arguments), 'load');
}
exports.open = open;

function close(window) {
  // unload event could happen so fast that it is not resolved
  // if we listen to unload after calling close()
  let p = promise(window, 'unload');
  window.close();
  return p;
}
exports.close = close;

function focus(window) {
  let p = onFocus(window);
  window.focus();
  return p;
}
exports.focus = focus;

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