/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Simon Bünzli <zeniko@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Utilities for JavaScript code to handle JSON content.
 * See http://www.json.org/ for comprehensive information about JSON.
 *
 * Import this module through
 *
 * Components.utils.import("resource://gre/modules/JSON.jsm");
 *
 * Usage:
 *
 * var newJSONString = JSON.toString( GIVEN_JAVASCRIPT_OBJECT );
 * var newJavaScriptObject = JSON.fromString( GIVEN_JSON_STRING );
 *
 * Note: For your own safety, Objects/Arrays returned by
 *       JSON.fromString aren't instanceof Object/Array.
 */

EXPORTED_SYMBOLS = ["JSON"];

// The following code is a loose adaption of Douglas Crockford's code
// from http://www.json.org/json.js (public domain'd)

// Notable differences:
// * Unserializable values such as |undefined| or functions aren't
//   silently dropped but always lead to a TypeError.
// * An optional key blacklist has been added to JSON.toString

var JSON = {
  /**
   * Converts a JavaScript object into a JSON string.
   *
   * @param aJSObject is the object to be converted
   * @param aKeysToDrop is an optional array of keys which will be
   *                    ignored in all objects during the serialization
   * @return the object's JSON representation
   *
   * Note: aJSObject MUST not contain cyclic references.
   */
  toString: function JSON_toString(aJSObject, aKeysToDrop) {
    // these characters have a special escape notation
    const charMap = { "\b": "\\b", "\t": "\\t", "\n": "\\n", "\f": "\\f",
                      "\r": "\\r", '"': '\\"', "\\": "\\\\" };
    
    // we use a single string builder for efficiency reasons
    var pieces = [];
    
    // this recursive function walks through all objects and appends their
    // JSON representation (in one or several pieces) to the string builder
    function append_piece(aObj) {
      if (typeof aObj == "boolean") {
        pieces.push(aObj ? "true" : "false");
      }
      else if (typeof aObj == "number" && isFinite(aObj)) {
        // there is no representation for infinite numbers or for NaN!
        pieces.push(aObj.toString());
      }
      else if (typeof aObj == "string") {
        aObj = aObj.replace(/[\\"\x00-\x1F\u0080-\uFFFF]/g, function($0) {
          // use the special escape notation if one exists, otherwise
          // produce a general unicode escape sequence
          return charMap[$0] ||
            "\\u" + ("0000" + $0.charCodeAt(0).toString(16)).slice(-4);
        });
        pieces.push('"' + aObj + '"')
      }
      else if (aObj === null) {
        pieces.push("null");
      }
      // if it looks like an array, treat it as such - this is required
      // for all arrays from either outside this module or a sandbox
      else if (aObj instanceof Array ||
               typeof aObj == "object" && "length" in aObj &&
               (aObj.length === 0 || aObj[aObj.length - 1] !== undefined)) {
        pieces.push("[");
        for (var i = 0; i < aObj.length; i++) {
          append_piece(aObj[i]);
          pieces.push(",");
        }
        if (pieces[pieces.length - 1] == ",")
          pieces.pop(); // drop the trailing colon
        pieces.push("]");
      }
      else if (typeof aObj == "object") {
        pieces.push("{");
        for (var key in aObj) {
          // allow callers to pass objects containing private data which
          // they don't want the JSON string to contain (so they don't
          // have to manually pre-process the object)
          if (aKeysToDrop && aKeysToDrop.indexOf(key) != -1)
            continue;
          
          append_piece(key.toString());
          pieces.push(":");
          append_piece(aObj[key]);
          pieces.push(",");
        }
        if (pieces[pieces.length - 1] == ",")
          pieces.pop(); // drop the trailing colon
        pieces.push("}");
      }
      else {
        throw new TypeError("No JSON representation for this object!");
      }
    }
    append_piece(aJSObject);
    
    return pieces.join("");
  },

  /**
   * Converts a JSON string into a JavaScript object.
   *
   * @param aJSONString is the string to be converted
   * @return a JavaScript object for the given JSON representation
   */
  fromString: function JSON_fromString(aJSONString) {
    if (!this.isMostlyHarmless(aJSONString))
      throw new SyntaxError("No valid JSON string!");
    
    var s = new Components.utils.Sandbox("about:blank");
    return Components.utils.evalInSandbox("(" + aJSONString + ")", s);
  },

  /**
   * Checks whether the given string contains potentially harmful
   * content which might be executed during its evaluation
   * (no parser, thus not 100% safe! Best to use a Sandbox for evaluation)
   *
   * @param aString is the string to be tested
   * @return a boolean
   */
  isMostlyHarmless: function JSON_isMostlyHarmless(aString) {
    const maybeHarmful = /[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/;
    const jsonStrings = /"(\\.|[^"\\\n\r])*"/g;
    
    return !maybeHarmful.test(aString.replace(jsonStrings, ""));
  }
};
