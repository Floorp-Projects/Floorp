/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  cssPropertiesSpec,
} = require("resource://devtools/shared/specs/css-properties.js");

loader.lazyRequireGetter(
  this,
  "cssColors",
  "resource://devtools/shared/css/color-db.js",
  true
);
loader.lazyRequireGetter(
  this,
  "CSS_TYPES",
  "resource://devtools/shared/css/constants.js",
  true
);
loader.lazyRequireGetter(
  this,
  "isCssVariable",
  "resource://devtools/shared/inspector/css-logic.js",
  true
);

/**
 * The CssProperties front provides a mechanism to have a one-time asynchronous
 * load of a CSS properties database. This is then fed into the CssProperties
 * interface that provides synchronous methods for finding out what CSS
 * properties the current server supports.
 */
class CssPropertiesFront extends FrontClassWithSpec(cssPropertiesSpec) {
  constructor(client, targetFront) {
    super(client, targetFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "cssPropertiesActor";
  }

  async initialize() {
    const db = await super.getCSSDatabase();
    this.cssProperties = new CssProperties(normalizeCssData(db));
  }

  destroy() {
    this.cssProperties = null;
    super.destroy();
  }
}

/**
 * Ask questions to a CSS database. This class does not care how the database
 * gets loaded in, only the questions that you can ask to it.
 * Prototype functions are bound to 'this' so they can be passed around as helper
 * functions.
 *
 * @param {Object} db
 *                 A database of CSS properties
 * @param {Object} inheritedList
 *                 The key is the property name, the value is whether or not
 *                 that property is inherited.
 */
function CssProperties(db) {
  this.properties = db.properties;

  this.isKnown = this.isKnown.bind(this);
  this.isInherited = this.isInherited.bind(this);
  this.supportsType = this.supportsType.bind(this);
}

CssProperties.prototype = {
  /**
   * Checks to see if the property is known by the browser. This function has
   * `this` already bound so that it can be passed around by reference.
   *
   * @param {String} property The property name to be checked.
   * @return {Boolean}
   */
  isKnown(property) {
    // Custom Property Names (aka CSS Variables) are case-sensitive; do not lowercase.
    property = property.startsWith("--") ? property : property.toLowerCase();
    return !!this.properties[property] || isCssVariable(property);
  },

  /**
   * Checks to see if the property is an inherited one.
   *
   * @param {String} property The property name to be checked.
   * @return {Boolean}
   */
  isInherited(property) {
    return this.properties[property]?.isInherited;
  },

  /**
   * Checks if the property supports the given CSS type.
   *
   * @param {String} property The property to be checked.
   * @param {String} type One of the values from InspectorPropertyType.
   * @return {Boolean}
   */
  supportsType(property, type) {
    const id = CSS_TYPES[type];
    return (
      this.properties[property] &&
      (this.properties[property].supports.includes(type) ||
        this.properties[property].supports.includes(id))
    );
  },

  /**
   * Gets the CSS values for a given property name.
   *
   * @param {String} property The property to use.
   * @return {Array} An array of strings.
   */
  getValues(property) {
    return this.properties[property] ? this.properties[property].values : [];
  },

  /**
   * Gets the CSS property names.
   *
   * @return {Array} An array of strings.
   */
  getNames(property) {
    return Object.keys(this.properties);
  },

  /**
   * Return a list of subproperties for the given property.  If |name|
   * does not name a valid property, an empty array is returned.  If
   * the property is not a shorthand property, then array containing
   * just the property itself is returned.
   *
   * @param {String} name The property to query
   * @return {Array} An array of subproperty names.
   */
  getSubproperties(name) {
    // Custom Property Names (aka CSS Variables) are case-sensitive; do not lowercase.
    name = name.startsWith("--") ? name : name.toLowerCase();
    if (this.isKnown(name)) {
      if (this.properties[name] && this.properties[name].subproperties) {
        return this.properties[name].subproperties;
      }
      return [name];
    }
    return [];
  },
};

/**
 * Even if the target has the cssProperties actor, the returned data may not be in the
 * same shape or have all of the data we need. This normalizes the data and fills in
 * any missing information like color values.
 *
 * @return {Object} The normalized CSS database.
 */
function normalizeCssData(db) {
  // If there is a `from` attributes, it means that it comes from RDP
  // and it is not the client `generateCssProperties()` object passed by tests.
  if (typeof db.from == "string") {
    // This is where to put backward compat tweaks here to support old runtimes.
  }

  reattachCssColorValues(db);

  return db;
}

/**
 * Color values are omitted to save on space. Add them back here.
 * @param {Object} The CSS database.
 */
function reattachCssColorValues(db) {
  if (db.properties.color.values[0] === "COLOR") {
    const colors = Object.keys(cssColors);

    for (const name in db.properties) {
      const property = db.properties[name];
      // "values" can be undefined if {name} was not found in CSS_PROPERTIES_DB.
      if (property.values && property.values[0] === "COLOR") {
        property.values.shift();
        property.values = property.values.concat(colors).sort();
      }
    }
  }
}

module.exports = {
  CssPropertiesFront,
  CssProperties,
  normalizeCssData,
};
registerFront(CssPropertiesFront);
