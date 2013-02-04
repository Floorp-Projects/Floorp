/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  "stability": "unstable"
};

let unescape = decodeURIComponent;
exports.unescape = unescape;

// encodes a string safely for application/x-www-form-urlencoded
// adheres to RFC 3986
// see https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/encodeURIComponent
function escape(query) {
  return encodeURIComponent(query).
    replace(/%20/g, '+').
    replace(/!/g, '%21').
    replace(/'/g, '%27').
    replace(/\(/g, '%28').
    replace(/\)/g, '%29').
    replace(/\*/g, '%2A');
}
exports.escape = escape;

// Converts an object of unordered key-vals to a string that can be passed
// as part of a request
function stringify(options, separator, assigner) {
  separator = separator || '&';
  assigner = assigner || '=';
  // Explicitly return null if we have null, and empty string, or empty object.
  if (!options)
    return '';

  // If content is already a string, just return it as is.
  if (typeof(options) == 'string')
    return options;

  // At this point we have a k:v object. Iterate over it and encode each value.
  // Arrays and nested objects will get encoded as needed. For example...
  //
  //   { foo: [1, 2, { omg: 'bbq', 'all your base!': 'are belong to us' }], bar: 'baz' }
  //
  // will be encoded as
  //
  //   foo[0]=1&foo[1]=2&foo[2][omg]=bbq&foo[2][all+your+base!]=are+belong+to+us&bar=baz
  //
  // Keys (including '[' and ']') and values will be encoded with
  // `escape` before returning.
  //
  // Execution was inspired by jQuery, but some details have changed and numeric
  // array keys are included (whereas they are not in jQuery).

  let encodedContent = [];
  function add(key, val) {
    encodedContent.push(escape(key) + assigner + escape(val));
  }

  function make(key, value) {
    if (value && typeof(value) === 'object')
      Object.keys(value).forEach(function(name) {
        make(key + '[' + name + ']', value[name]);
      });
    else
      add(key, value);
  }

  Object.keys(options).forEach(function(name) { make(name, options[name]); });
  return encodedContent.join(separator);

  //XXXzpao In theory, we can just use a FormData object on 1.9.3, but I had
  //        trouble getting that working. It would also be nice to stay
  //        backwards-compat as long as possible. Keeping this in for now...
  // let formData = Cc['@mozilla.org/files/formdata;1'].
  //                createInstance(Ci.nsIDOMFormData);
  // for ([k, v] in Iterator(content)) {
  //   formData.append(k, v);
  // }
  // return formData;
}
exports.stringify = stringify;

// Exporting aliases that nodejs implements just for the sake of
// interoperability.
exports.encode = stringify;
exports.serialize = stringify;

// Note: That `stringify` and `parse` aren't bijective as we use `stringify`
// as it was implement in request module, but implement `parse` to match nodejs
// behavior.
// TODO: Make `stringify` implement API as in nodejs and figure out backwards
// compatibility.
function parse(query, separator, assigner) {
  separator = separator || '&';
  assigner = assigner || '=';
  let result = {};

  if (typeof query !== 'string' || query.length === 0)
    return result;

  query.split(separator).forEach(function(chunk) {
    let pair = chunk.split(assigner);
    let key = unescape(pair[0]);
    let value = unescape(pair.slice(1).join(assigner));

    if (!(key in result))
      result[key] = value;
    else if (Array.isArray(result[key]))
      result[key].push(value);
    else
      result[key] = [result[key], value];
  });

  return result;
};
exports.parse = parse;
// Exporting aliases that nodejs implements just for the sake of
// interoperability.
exports.decode = parse;
