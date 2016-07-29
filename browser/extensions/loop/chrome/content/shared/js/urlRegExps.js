"use strict"; // This is derived from Diego Perini's code,
// currently available at https://gist.github.com/dperini/729294

// Regular Expression for URL validation
//
// Original Author: Diego Perini
// License: MIT
//
// Copyright (c) 2010-2013 Diego Perini (http://www.iport.it)
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.urlRegExps = function () {

  "use strict";

  // Some https://wiki.mozilla.org/Loop/Development/RegExpDebugging for tools
  // if you need to debug changes to this:

  var fullUrlMatch = new RegExp(
  // Protocol identifier.
  "(?:(?:https?|ftp)://)" + 
  // User:pass authentication.
  "((?:\\S+(?::\\S*)?@)?" + 
  "(?:" + 
  // IP address dotted notation octets:
  // excludes loopback network 0.0.0.0,
  // excludes reserved space >= 224.0.0.0,
  // excludes network & broadcast addresses,
  // (first & last IP address of each class).
  "(?:[1-9]\\d?|1\\d\\d|2[01]\\d|22[0-3])" + 
  "(?:\\.(?:1?\\d{1,2}|2[0-4]\\d|25[0-5])){2}" + 
  "(?:\\.(?:[1-9]\\d?|1\\d\\d|2[0-4]\\d|25[0-4]))" + 
  "|" + 
  // Host name.
  "(?:(?:[a-z\\u00a1-\\uffff0-9]-*)*[a-z\\u00a1-\\uffff0-9]+)" + 
  // Domain name.
  "(?:\\.(?:[a-z\\u00a1-\\uffff0-9]-*)*[a-z\\u00a1-\\uffff0-9]+)*" + 
  // TLD identifier.
  "(?:\\.(?:[a-z\\u00a1-\\uffff]{2,})))" + 
  // Port number.
  "(?::\\d{2,5})?" + 
  // Resource path.
  "(?:[/?#]\\S*)?)", "i");

  return { 
    fullUrlMatch: fullUrlMatch };}();
