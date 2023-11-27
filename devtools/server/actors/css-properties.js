/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  cssPropertiesSpec,
} = require("resource://devtools/shared/specs/css-properties.js");

const { cssColors } = require("resource://devtools/shared/css/color-db.js");

loader.lazyRequireGetter(
  this,
  "CSS_TYPES",
  "resource://devtools/shared/css/constants.js",
  true
);

class CssPropertiesActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, cssPropertiesSpec);
    this.targetActor = targetActor;
  }

  getCSSDatabase() {
    const properties = generateCssProperties(this.targetActor.window.document);

    return { properties };
  }
}
exports.CssPropertiesActor = CssPropertiesActor;

/**
 * Generate the CSS properties object. Every key is the property name, while
 * the values are objects that contain information about that property.
 *
 * @param {Document} doc
 * @return {Object}
 */
function generateCssProperties(doc) {
  const properties = {};
  const propertyNames = InspectorUtils.getCSSPropertyNames({
    includeAliases: true,
  });
  const colors = Object.keys(cssColors);

  propertyNames.forEach(name => {
    // Get the list of CSS types this property supports.
    const supports = [];
    for (const type in CSS_TYPES) {
      if (safeCssPropertySupportsType(name, type)) {
        supports.push(type);
      }
    }

    // Don't send colors over RDP, these will be re-attached by the front.
    let values = InspectorUtils.getCSSValuesForProperty(name);
    if (values.includes("aliceblue")) {
      values = values.filter(x => !colors.includes(x));
      values.unshift("COLOR");
    }

    const subproperties = InspectorUtils.getSubpropertiesForCSSProperty(name);

    properties[name] = {
      isInherited: InspectorUtils.isInheritedProperty(doc, name),
      values,
      supports,
      subproperties,
    };
  });

  return properties;
}
exports.generateCssProperties = generateCssProperties;

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
    InspectorUtils.cssPropertyIsShorthand(name);
    return true;
  } catch (e) {
    return false;
  }
}

exports.isCssPropertyKnown = isCssPropertyKnown;

/**
 * A wrapper for InspectorUtils.cssPropertySupportsType that ignores invalid
 * properties.
 *
 * @param {String} name The property name.
 * @param {number} type The type tested for support.
 * @return {Boolean} Whether the property supports the type.
 *        If the property is unknown, false is returned.
 */
function safeCssPropertySupportsType(name, type) {
  try {
    return InspectorUtils.cssPropertySupportsType(name, type);
  } catch (e) {
    return false;
  }
}
