/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");

loader.lazyGetter(this, "DOMUtils", () => {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

const protocol = require("devtools/shared/protocol");
const { ActorClassWithSpec, Actor } = protocol;
const { cssPropertiesSpec } = require("devtools/shared/specs/css-properties");

var CssPropertiesActor = exports.CssPropertiesActor = ActorClassWithSpec(cssPropertiesSpec, {
  typeName: "cssProperties",

  initialize: function(conn, parent) {
    Actor.prototype.initialize.call(this, conn);
    this.parent = parent;
  },

  destroy: function() {
    Actor.prototype.destroy.call(this);
  },

  getCSSDatabase: function() {
    const propertiesList = DOMUtils.getCSSPropertyNames(DOMUtils.INCLUDE_ALIASES);
    return { propertiesList };
  }
});

/**
 * Test if a CSS is property is known using server-code.
 *
 * @param {string} name
 * @return {Boolean}
 */
function isCssPropertyKnown(name) {
  try {
    // If the property name is unknown, the cssPropertyIsShorthand
    // will throw an exception.  But if it is known, no exception will
    // be thrown; so we just ignore the return value.
    DOMUtils.cssPropertyIsShorthand(name);
    return true;
  } catch (e) {
    return false;
  }
}

exports.isCssPropertyKnown = isCssPropertyKnown
