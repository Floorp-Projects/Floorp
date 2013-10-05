/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");
const { StringBundle } = require("sdk/deprecated/app-strings");

exports.testStringBundle = function(assert) {
  let url = "chrome://global/locale/security/caps.properties";

  let strings = StringBundle(url);

  assert.equal(strings.url, url,
                   "'url' property contains correct URL of string bundle");

  let appLocale = Cc["@mozilla.org/intl/nslocaleservice;1"].
                  getService(Ci.nsILocaleService).
                  getApplicationLocale();

  let stringBundle = Cc["@mozilla.org/intl/stringbundle;1"].
                     getService(Ci.nsIStringBundleService).
                     createBundle(url, appLocale);

  let (name = "CheckMessage") {
    assert.equal(strings.get(name), stringBundle.GetStringFromName(name),
                 "getting a string returns the string");
  }

  let (name = "CreateWrapperDenied", args = ["foo"]) {
    assert.equal(strings.get(name, args),
                 stringBundle.formatStringFromName(name, args, args.length),
                 "getting a formatted string returns the formatted string");
  }

  assert.throws(function () strings.get("nonexistentString"),
                RegExp("String 'nonexistentString' could not be retrieved from " +
                       "the bundle due to an unknown error \\(it doesn't exist\\?\\)\\."),
                "retrieving a nonexistent string throws an exception");

  let a = [], b = [];
  let enumerator = stringBundle.getSimpleEnumeration();
  while (enumerator.hasMoreElements()) {
    let elem = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
    a.push([elem.key, elem.value]);
  }

  for (let key in strings) {
    b.push([ key, strings.get(key) ]);
  }

  // Sort the arrays, because we don't assume enumeration has a set order.
  // Sort compares [key, val] as string "key,val", which sorts the way we want
  // it to, so there is no need to provide a custom compare function.
  a.sort();
  b.sort();

  assert.equal(a.length, b.length,
               "the iterator returns the correct number of items");

  for (let i = 0; i < a.length; i++) {
    assert.equal(a[i][0], b[i][0], "the iterated string's name is correct");
    assert.equal(a[i][1], b[i][1],
                     "the iterated string's value is correct");
  }
};

require("sdk/test").run(exports);
