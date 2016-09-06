/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { FrontClassWithSpec, Front } = require("devtools/shared/protocol");
const { cssPropertiesSpec } = require("devtools/shared/specs/css-properties");
const { Task } = require("devtools/shared/task");
const { CSS_PROPERTIES_DB } = require("devtools/shared/css-properties-db");
const { cssColors } = require("devtools/shared/css-color-db");

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
var IS_VARIABLE_TOKEN = new RegExp(`^--(${FIRST_CHAR})(${TRAILING_CHAR})*$`,
                                   "i");
/**
 * Check that this is a CSS variable.
 *
 * @param {String} input
 * @return {Boolean}
 */
function isCssVariable(input) {
  return !!input.match(IS_VARIABLE_TOKEN);
}

var cachedCssProperties = new WeakMap();

/**
 * The CssProperties front provides a mechanism to have a one-time asynchronous
 * load of a CSS properties database. This is then fed into the CssProperties
 * interface that provides synchronous methods for finding out what CSS
 * properties the current server supports.
 */
const CssPropertiesFront = FrontClassWithSpec(cssPropertiesSpec, {
  initialize: function (client, { cssPropertiesActor }) {
    Front.prototype.initialize.call(this, client, {actor: cssPropertiesActor});
    this.manage(this);
  }
});

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
    return !!this.properties[property] || isCssVariable(property);
  },

  /**
   * Checks to see if the property is an inherited one.
   *
   * @param {String} property The property name to be checked.
   * @return {Boolean}
   */
  isInherited(property) {
    return this.properties[property] && this.properties[property].isInherited;
  },

  /**
   * Checks if the property supports the given CSS type.
   * CSS types should come from devtools/shared/css-properties-db.js' CSS_TYPES.
   *
   * @param {String} property The property to be checked.
   * @param {Number} type One of the type values from CSS_TYPES.
   * @return {Boolean}
   */
  supportsType(property, type) {
    return this.properties[property] && this.properties[property].supports.includes(type);
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
  }
};

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
const initCssProperties = Task.async(function* (toolbox) {
  const client = toolbox.target.client;
  if (cachedCssProperties.has(client)) {
    return cachedCssProperties.get(client);
  }

  let db, front;

  // Get the list dynamically if the cssProperties actor exists.
  if (toolbox.target.hasActor("cssProperties")) {
    front = CssPropertiesFront(client, toolbox.target.form);
    const serverDB = yield front.getCSSDatabase(getClientBrowserVersion(toolbox));

    // The serverDB will be blank if the browser versions match, so use the static list.
    if (!serverDB.properties && !serverDB.margin) {
      db = CSS_PROPERTIES_DB;
    } else {
      db = serverDB;
    }
  } else {
    // The target does not support this actor, so require a static list of supported
    // properties.
    db = CSS_PROPERTIES_DB;
  }

  const cssProperties = new CssProperties(normalizeCssData(db));
  cachedCssProperties.set(client, {cssProperties, front});
  return {cssProperties, front};
});

/**
 * Synchronously get a cached and initialized CssProperties.
 *
 * @param {Toolbox} The current toolbox.
 * @returns {CssProperties}
 */
function getCssProperties(toolbox) {
  if (!cachedCssProperties.has(toolbox.target.client)) {
    throw new Error("The CSS database has not been initialized, please make " +
                    "sure initCssDatabase was called once before for this " +
                    "toolbox.");
  }
  return cachedCssProperties.get(toolbox.target.client).cssProperties;
}

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
 * Get the current browser version.
 * @returns {string} The browser version.
 */
function getClientBrowserVersion(toolbox) {
  const regexResult = toolbox.win.navigator
                             .userAgent.match(/Firefox\/(\d+)\.\d/);
  return Array.isArray(regexResult) ? regexResult[1] : "0";
}

/**
 * Even if the target has the cssProperties actor, the returned data may not be in the
 * same shape or have all of the data we need. This normalizes the data and fills in
 * any missing information like color values.
 *
 * @return {Object} The normalized CSS database.
 */
function normalizeCssData(db) {
  if (db !== CSS_PROPERTIES_DB) {
    // Firefox 49's getCSSDatabase() just returned the properties object, but
    // now it returns an object with multiple types of CSS information.
    if (!db.properties) {
      db = { properties: db };
    }

    // Fill in any missing DB information from the static database.
    db = Object.assign({}, CSS_PROPERTIES_DB, db);

    // Add "supports" information to the css properties if it's missing.
    if (!db.properties.color.supports) {
      for (let name in db.properties) {
        if (typeof CSS_PROPERTIES_DB.properties[name] === "object") {
          db.properties[name].supports = CSS_PROPERTIES_DB.properties[name].supports;
        }
      }
    }

    // Add "values" information to the css properties if it's missing.
    if (!db.properties.color.values) {
      for (let name in db.properties) {
        if (typeof CSS_PROPERTIES_DB.properties[name] === "object") {
          db.properties[name].values = CSS_PROPERTIES_DB.properties[name].values;
        }
      }
    }
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

    for (let name in db.properties) {
      const property = db.properties[name];
      if (property.values[0] === "COLOR") {
        property.values.shift();
        property.values = property.values.concat(colors).sort();
      }
    }
  }
}

module.exports = {
  CssPropertiesFront,
  CssProperties,
  getCssProperties,
  getClientCssProperties,
  initCssProperties
};
