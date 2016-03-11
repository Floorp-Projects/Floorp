/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  "stability": "stable"
};

const { Cc, Ci } = require('chrome');
const { get, set, exists } = Cc['@mozilla.org/process/environment;1'].
                             getService(Ci.nsIEnvironment);

exports.env = new Proxy({}, {
  deleteProperty(target, property) {
    set(property, null);
    return true;
  },

  get(target, property, receiver) {
    return get(property) || undefined;
  },

  has(target, property) {
    return exists(property);
  },

  set(target, property, value, receiver) {
    set(property, value);
    return true;
  }
});
