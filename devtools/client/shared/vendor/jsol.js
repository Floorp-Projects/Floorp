/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
(function () {
  /**
   JSOL stands for JavaScript Object Literal which is a string representing
   an object in JavaScript syntax.

   For example:

   {foo:"bar"} is equivalent to {"foo":"bar"} in JavaScript. Both are valid JSOL.

   Note that {"foo":"bar"} is proper JSON[1] therefore you can use one of the many
   JSON parsers out there like json2.js[2] or even the native browser's JSON parser,
   if available.

   However, {foo:"bar"} is NOT proper JSON but valid Javascript syntax for
   representing an object with one key, "foo" and its value, "bar".
   Using a JSON parser is not an option since this is NOT proper JSON.

   You can use JSOL.parse to safely parse any string that reprsents a JavaScript Object Literal.
   JSOL.parse will throw an Invalid JSOL exception on function calls, function declarations and variable references.

   Examples:

   JSOL.parse('{foo:"bar"}');  // valid

   JSOL.parse('{evil:(function(){alert("I\'m evil");})()}');  // invalid function calls

   JSOL.parse('{fn:function() { }}'); // invalid function declarations

   var bar = "bar";
   JSOL.parse('{foo:bar}');  // invalid variable references

   [1] http://www.json.org
   [2] http://www.json.org/json2.js
   */
  var trim =  /^(\s|\u00A0)+|(\s|\u00A0)+$/g; // Used for trimming whitespace
  var JSOL = {
    parse: function(text) {
      // make sure text is a "string"
      if (typeof text !== "string" || !text) {
        return null;
      }
      // Make sure leading/trailing whitespace is removed
      text = text.replace(trim, "");
      // Make sure the incoming text is actual JSOL (or Javascript Object Literal)
      // Logic borrowed from http://json.org/json2.js
      if ( /^[\],:{}\s]*$/.test(text.replace(/\\(?:["\\\/bfnrt]|u[0-9a-fA-F]{4})/g, "@")
           .replace(/"[^"\\\n\r]*"|true|false|null|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?/g, "]")
           .replace(/(?:^|:|,)(?:\s*\[)+/g, ":")
           /** everything up to this point is json2.js **/
           /** this is the 5th stage where it accepts unquoted keys **/
           .replace(/\w*\s*\:/g, ":")) ) {
        return (new Function("return " + text))();
      }
      else {
        throw("Invalid JSOL: " + text);
      }
    }
  };

  if (typeof define === "function" && define.amd) {
    define(JSOL);
  } else if (typeof module === "object" && module.exports) {
    module.exports = JSOL;
  } else {
    this.JSOL = JSOL;
  }
})();
