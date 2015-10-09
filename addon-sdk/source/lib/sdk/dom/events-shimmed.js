/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'unstable'
};

const events = require('./events.js');

exports.emit = (element, type, obj) => events.emit(element, type, obj, true);
exports.on = (element, type, listener, capture) => events.on(element, type, listener, capture, true);
exports.once = (element, type, listener, capture) => events.once(element, type, listener, capture, true);
exports.removeListener = (element, type, listener, capture) => events.removeListener(element, type, listener, capture, true);
exports.removed = events.removed;
exports.when = (element, eventName, capture) => events.when(element, eventName, capture ? capture : false, true);
