/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { cssPropertiesSpec } = require("devtools/shared/specs/css-properties");

loader.lazyRequireGetter(
  this,
  "cssColors",
  "devtools/shared/css/color-db",
  true
);
loader.lazyRequireGetter(
  this,
  "CSS_PROPERTIES_DB",
  "devtools/shared/css/properties-db",
  true
);
loader.lazyRequireGetter(
  this,
  "CSS_TYPES",
  "devtools/shared/css/constants",
  true
);

/**
 * Build up a regular expression that matches a CSS variable token. This is an
 * ident token that starts with two dashes "--".
 *
 * https://www.w3.org/TR/css-syntax-3/#ident-token-diagram
 */
var NON_ASCII = "[^\\x00-\\x7F]";
var ESCAPE = "\\\\[^\n\r]";
var FIRST_CHAR = ["[_a-z]", NON_ASCII, ESCAPE].join("|");
var TRAILING_CHAR = ["[_a-z0-9-]", NON_ASCII, ESCAPE].join("|");
var IS_VARIABLE_TOKEN = new RegExp(
  `^--(${FIRST_CHAR})(${TRAILING_CHAR})*$`,
  "i"
);

var cachedCssProperties = new WeakMap();

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
  this.pseudoElements = db.pseudoElements;

  // supported feature
  this.cssColor4ColorFunction = hasFeature(
    db.supportedFeature,
    "css-color-4-color-function"
  );

  this.isKnown = this.isKnown.bind(this);
  this.isInherited = this.isInherited.bind(this);
  this.supportsType = this.supportsType.bind(this);
  this.supportsCssColor4ColorFunction = this.supportsCssColor4ColorFunction.bind(
    this
  );
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
    return (
      (this.properties[property] && this.properties[property].isInherited) ||
      isCssVariable(property)
    );
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

  /**
   * Checking for the css-color-4 color function support.
   *
   * @return {Boolean} Return true if the server supports css-color-4 color function.
   */
  supportsCssColor4ColorFunction() {
    return this.cssColor4ColorFunction;
  },
};

/**
 * Check that this is a CSS variable.
 *
 * @param {String} input
 * @return {Boolean}
 */
function isCssVariable(input) {
  return !!input.match(IS_VARIABLE_TOKEN);
}

/**
 * Query the feature supporting status in the featureSet.
 *
 * @param {Hashmap} featureSet the feature set hashmap
 * @param {String} feature the feature name string
 * @return {Boolean} has the feature or not
 */
function hasFeature(featureSet, feature) {
  if (feature in featureSet) {
    return featureSet[feature];
  }
  return false;
}

/**
 * Create a CssProperties object with a fully loaded CSS database. The
 * CssProperties interface can be queried synchronously, but the initialization
 * is potentially async and should be handled up-front when the tool is created.
 *
 * The front is returned only with this function so that it can be destroyed
 * once the toolbox is destroyed.
 *
 * @param {Toolbox} The current toolbox.
 * @returns {Promise} Resolves to {cssProperties, cssPropertiesFront}.
 */
const initCssProperties = async function(toolbox) {
  const client = toolbox.target.client;
  if (cachedCssProperties.has(client)) {
    return cachedCssProperties.get(client);
  }

  // Get the list dynamically if the cssProperties actor exists.
  const front = await toolbox.target.getFront("cssProperties");
  const db = await front.getCSSDatabase();

  const cssProperties = new CssProperties(normalizeCssData(db));
  cachedCssProperties.set(client, { cssProperties, front });
  return { cssProperties, front };
};

/**
 * Get a client-side CssProperties. This is useful for dependencies in tests, or parts
 * of the codebase that don't particularly need to match every known CSS property on
 * the target.
 * @return {CssProperties}
 */
function getClientCssProperties() {
  return new CssProperties(normalizeCssData(CSS_PROPERTIES_DB));
}

/**
 * Even if the target has the cssProperties actor, the returned data may not be in the
 * same shape or have all of the data we need. This normalizes the data and fills in
 * any missing information like color values.
 *
 * @return {Object} The normalized CSS database.
 */
function normalizeCssData(db) {
  // If there is a `from` attributes, it means that it comes from RDP
  // and it is not the client CSS_PROPERTIES_DB object.
  // (prevent comparing to CSS_PROPERTIES_DB to avoid loading client database)
  if (typeof db.from == "string") {
    // Firefox 49's getCSSDatabase() just returned the properties object, but
    // now it returns an object with multiple types of CSS information.
    if (!db.properties) {
      db = { properties: db };
    }

    const missingSupports = !db.properties.color.supports;
    const missingValues = !db.properties.color.values;
    const missingSubproperties = !db.properties.background.subproperties;
    const missingIsInherited = !db.properties.font.isInherited;

    const missingSomething =
      missingSupports ||
      missingValues ||
      missingSubproperties ||
      missingIsInherited;

    if (missingSomething) {
      for (const name in db.properties) {
        // Skip the current property if we can't find it in CSS_PROPERTIES_DB.
        if (typeof CSS_PROPERTIES_DB.properties[name] !== "object") {
          continue;
        }

        // Add "supports" information to the css properties if it's missing.
        if (missingSupports) {
          db.properties[name].supports =
            CSS_PROPERTIES_DB.properties[name].supports;
        }
        // Add "values" information to the css properties if it's missing.
        if (missingValues) {
          db.properties[name].values =
            CSS_PROPERTIES_DB.properties[name].values;
        }
        // Add "subproperties" information to the css properties if it's missing.
        if (missingSubproperties) {
          db.properties[name].subproperties =
            CSS_PROPERTIES_DB.properties[name].subproperties;
        }
        // Add "isInherited" information to the css properties if it's missing.
        if (missingIsInherited) {
          db.properties[name].isInherited =
            CSS_PROPERTIES_DB.properties[name].isInherited;
        }
      }
    }
  }

  reattachCssColorValues(db);

  // If there is no supportedFeature in db, create an empty one.
  if (!db.supportedFeature) {
    db.supportedFeature = {};
  }

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
  getClientCssProperties,
  initCssProperties,
  isCssVariable,
};
registerFront(CssPropertiesFront);
