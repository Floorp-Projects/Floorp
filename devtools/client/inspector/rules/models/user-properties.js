/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Store of CSSStyleDeclarations mapped to properties that have been changed by
 * the user.
 */
function UserProperties() {
  this.map = new Map();
}

UserProperties.prototype = {
  /**
   * Get a named property for a given CSSStyleDeclaration.
   *
   * @param {CSSStyleDeclaration} style
   *        The CSSStyleDeclaration against which the property is mapped.
   * @param {String} name
   *        The name of the property to get.
   * @param {String} value
   *        Default value.
   * @return {String}
   *        The property value if it has previously been set by the user, null
   *        otherwise.
   */
  getProperty: function(style, name, value) {
    const key = this.getKey(style);
    const entry = this.map.get(key, null);

    if (entry && name in entry) {
      return entry[name];
    }
    return value;
  },

  /**
   * Set a named property for a given CSSStyleDeclaration.
   *
   * @param {CSSStyleDeclaration} style
   *        The CSSStyleDeclaration against which the property is to be mapped.
   * @param {String} name
   *        The name of the property to set.
   * @param {String} userValue
   *        The value of the property to set.
   */
  setProperty: function(style, name, userValue) {
    const key = this.getKey(style, name);
    const entry = this.map.get(key, null);

    if (entry) {
      entry[name] = userValue;
    } else {
      const props = {};
      props[name] = userValue;
      this.map.set(key, props);
    }
  },

  /**
   * Check whether a named property for a given CSSStyleDeclaration is stored.
   *
   * @param {CSSStyleDeclaration} style
   *        The CSSStyleDeclaration against which the property would be mapped.
   * @param {String} name
   *        The name of the property to check.
   */
  contains: function(style, name) {
    const key = this.getKey(style, name);
    const entry = this.map.get(key, null);
    return !!entry && name in entry;
  },

  getKey: function(style, name) {
    return style.actorID + ":" + name;
  },

  clear: function() {
    this.map.clear();
  }
};

module.exports = UserProperties;
