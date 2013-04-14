/* vim:set ts=2 sw=2 sts=2 expandtab */
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

exports.env = Proxy.create({
  // XPCOM does not provides a way to enumerate environment variables, so we
  // just don't support enumeration.
  getPropertyNames: function() [],
  getOwnPropertyNames: function() [],
  enumerate: function() [],
  keys: function() [],
  // We do not support freezing, cause it would make it impossible to set new
  // environment variables.
  fix: function() undefined,
  // We present all environment variables as own properties of this object,
  // so we just delegate this call to `getOwnPropertyDescriptor`.
  getPropertyDescriptor: function(name) this.getOwnPropertyDescriptor(name),
  // If environment variable with this name is defined, we generate proprety
  // descriptor for it, otherwise fall back to `undefined` so that for consumer
  // this property does not exists.
  getOwnPropertyDescriptor: function(name) {
    return !exists(name) ? undefined : {
      value: get(name),
      enumerable: false,    // Non-enumerable as we don't support enumeration.
      configurable: true,   // Configurable as it may be deleted.
      writable: true        // Writable as we do support set.
    }
  },

  // New environment variables can be defined just by defining properties
  // on this object.
  defineProperty: function(name, { value }) set(name, value),
  delete: function(name) {
    set(name, null);
    return true;
  },

  // We present all properties as own, there for we just delegate to `hasOwn`.
  has: function(name) this.hasOwn(name),
  // We do support checks for existence of an environment variable, via `in`
  // operator on this object.
  hasOwn: function(name) exists(name),

  // On property get / set we do read / write appropriate environment variables,
  // please note though, that variables with names of standard object properties
  // intentionally (so that this behaves as normal object) can not be
  // read / set.
  get: function(proxy, name) Object.prototype[name] || get(name) || undefined,
  set: function(proxy, name, value) Object.prototype[name] || set(name, value)
});
