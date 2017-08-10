/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'unstable'
};

const events = require('./events.js');

exports.emit = (type, event) => events.emit(type, event, true);
exports.on = (type, listener, strong) => events.on(type, listener, strong, true);
exports.once = (type, listener) => events.once(type, listener, true);
exports.off = (type, listener) => events.off(type, listener, true);
