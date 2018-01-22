/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PREF_LOGLEVEL           = "browser.policies.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = Cu.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    prefix: "PoliciesValidator.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

this.EXPORTED_SYMBOLS = ["PoliciesValidator"];

this.PoliciesValidator = {
  validateAndParseParameters(param, properties) {
    return validateAndParseParamRecursive(param, properties);
  }
};

function validateAndParseParamRecursive(param, properties) {
  if (properties.enum) {
    if (properties.enum.includes(param)) {
      return [true, param];
    }
    return [false, null];
  }

  log.debug(`checking @${param}@ for type ${properties.type}`);
  switch (properties.type) {
    case "boolean":
    case "number":
    case "integer":
    case "string":
    case "URL":
    case "origin":
      return validateAndParseSimpleParam(param, properties.type);

    case "array":
      if (!Array.isArray(param)) {
        log.error("Array expected but not received");
        return [false, null];
      }

      let parsedArray = [];
      for (let item of param) {
        log.debug(`in array, checking @${item}@ for type ${properties.items.type}`);
        let [valid, parsedValue] = validateAndParseParamRecursive(item, properties.items);
        if (!valid) {
          return [false, null];
        }

        parsedArray.push(parsedValue);
      }

      return [true, parsedArray];

    case "object": {
      if (typeof(param) != "object") {
        log.error("Object expected but not received");
        return [false, null];
      }

      let parsedObj = {};
      for (let property of Object.keys(properties.properties)) {
        log.debug(`in object, for property ${property} checking @${param[property]}@ for type ${properties.properties[property].type}`);
        let [valid, parsedValue] = validateAndParseParamRecursive(param[property], properties.properties[property]);
        if (!valid) {
          return [false, null];
        }

        parsedObj[property] = parsedValue;
      }

      return [true, parsedObj];
    }
  }

  return [false, null];
}

function validateAndParseSimpleParam(param, type) {
  let valid = false;
  let parsedParam = param;

  switch (type) {
    case "boolean":
    case "number":
    case "string":
      valid = (typeof(param) == type);
      break;

    // integer is an alias to "number" that some JSON schema tools use
    case "integer":
      valid = (typeof(param) == "number");
      break;

    case "origin":
      if (typeof(param) != "string") {
        break;
      }

      try {
        parsedParam = Services.io.newURI(param);

        let pathQueryRef = parsedParam.pathQueryRef;
        // Make sure that "origin" types won't accept full URLs.
        if (pathQueryRef != "/" && pathQueryRef != "") {
          valid = false;
        } else {
          valid = true;
        }
      } catch (ex) {
        valid = false;
      }
      break;

    case "URL":
      if (typeof(param) != "string") {
        break;
      }

      try {
        parsedParam = Services.io.newURI(param);
        valid = true;
      } catch (ex) {
        valid = false;
      }
      break;
  }

  return [valid, parsedParam];
}
