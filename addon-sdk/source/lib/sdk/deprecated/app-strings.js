/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "deprecated"
};

const {Cc,Ci} = require("chrome");
const apiUtils = require("./api-utils");

/**
 * A bundle of strings.
 *
 * @param url {String}
 *        the URL of the string bundle
 */
exports.StringBundle = apiUtils.publicConstructor(function StringBundle(url) {

  let stringBundle = Cc["@mozilla.org/intl/stringbundle;1"].
                     getService(Ci.nsIStringBundleService).
                     createBundle(url);

  this.__defineGetter__("url", function () url);

  /**
   * Get a string from the bundle.
   *
   * @param name {String}
   *        the name of the string to get
   * @param args {array} [optional]
   *        an array of arguments that replace occurrences of %S in the string
   *
   * @returns {String} the value of the string
   */
  this.get = function strings_get(name, args) {
    try {
      if (args)
        return stringBundle.formatStringFromName(name, args, args.length);
      else
        return stringBundle.GetStringFromName(name);
    }
    catch(ex) {
      // f.e. "Component returned failure code: 0x80004005 (NS_ERROR_FAILURE)
      // [nsIStringBundle.GetStringFromName]"
      throw new Error("String '" + name + "' could not be retrieved from the " +
                      "bundle due to an unknown error (it doesn't exist?).");
    }
  },

  /**
   * Iterate the strings in the bundle.
   *
   */
  apiUtils.addIterator(
    this,
    function keysValsGen() {
      let enumerator = stringBundle.getSimpleEnumeration();
      while (enumerator.hasMoreElements()) {
        let elem = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
        yield [elem.key, elem.value];
      }
    }
  );
});
