/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const prefs = require("sdk/preferences/service");
const Branch = prefs.Branch;
const { Cc, Ci, Cu } = require("chrome");
const BundleService = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);

const specialChars = "!@#$%^&*()_-=+[]{}~`\'\"<>,./?;:";

exports.testReset = function(test) {
  prefs.reset("test_reset_pref");
  test.assertEqual(prefs.has("test_reset_pref"), false);
  test.assertEqual(prefs.isSet("test_reset_pref"), false);
  prefs.set("test_reset_pref", 5);
  test.assertEqual(prefs.has("test_reset_pref"), true);
  test.assertEqual(prefs.isSet("test_reset_pref"), true);
  test.assertEqual(prefs.keys("test_reset_pref").toString(), "test_reset_pref");
};

exports.testGetAndSet = function(test) {
  let svc = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch(null);
  svc.setCharPref("test_set_get_pref", "a normal string");
  test.assertEqual(prefs.get("test_set_get_pref"), "a normal string",
                   "preferences-service should read from " +
                   "application-wide preferences service");

  prefs.set("test_set_get_pref.integer", 1);
  test.assertEqual(prefs.get("test_set_get_pref.integer"), 1,
                   "set/get integer preference should work");

  test.assertEqual(
      prefs.keys("test_set_get_pref").sort().toString(),
      ["test_set_get_pref.integer","test_set_get_pref"].sort().toString());

  prefs.set("test_set_get_number_pref", 42);
  test.assertRaises(
    function() { prefs.set("test_set_get_number_pref", 3.14159); },
    "cannot store non-integer number: 3.14159",
    "setting a float preference should raise an error"
  );
  test.assertEqual(prefs.get("test_set_get_number_pref"), 42,
                   "bad-type write attempt should not overwrite");

  // 0x80000000 (no), 0x7fffffff (yes), -0x80000000 (yes), -0x80000001 (no)
  test.assertRaises(
    function() { prefs.set("test_set_get_number_pref", Math.pow(2, 31)); },
    ("you cannot set the test_set_get_number_pref pref to the number " +
     "2147483648, as number pref values must be in the signed 32-bit " +
     "integer range -(2^31) to 2^31-1.  To store numbers outside that " +
     "range, store them as strings."),
    "setting an int pref outside the range -(2^31) to 2^31-1 shouldn't work"
  );
  test.assertEqual(prefs.get("test_set_get_number_pref"), 42,
                   "out-of-range write attempt should not overwrite 1");
  prefs.set("test_set_get_number_pref", Math.pow(2, 31)-1);
  test.assertEqual(prefs.get("test_set_get_number_pref"), 0x7fffffff,
                   "in-range write attempt should work 1");
  prefs.set("test_set_get_number_pref", -Math.pow(2, 31));
  test.assertEqual(prefs.get("test_set_get_number_pref"), -0x80000000,
                   "in-range write attempt should work 2");
  test.assertRaises(
    function() { prefs.set("test_set_get_number_pref", -0x80000001); },
    ("you cannot set the test_set_get_number_pref pref to the number " +
     "-2147483649, as number pref values must be in the signed 32-bit " +
     "integer range -(2^31) to 2^31-1.  To store numbers outside that " +
     "range, store them as strings."),
    "setting an int pref outside the range -(2^31) to 2^31-1 shouldn't work"
  );
  test.assertEqual(prefs.get("test_set_get_number_pref"), -0x80000000,
                   "out-of-range write attempt should not overwrite 2");


  prefs.set("test_set_get_pref.string", "foo");
  test.assertEqual(prefs.get("test_set_get_pref.string"), "foo",
                   "set/get string preference should work");

  prefs.set("test_set_get_pref.boolean", true);
  test.assertEqual(prefs.get("test_set_get_pref.boolean"), true,
                   "set/get boolean preference should work");

  prefs.set("test_set_get_unicode_pref", String.fromCharCode(960));
  test.assertEqual(prefs.get("test_set_get_unicode_pref"),
                   String.fromCharCode(960),
                   "set/get unicode preference should work");

  var unsupportedValues = [null, [], undefined];
  unsupportedValues.forEach(
    function(value) {
      test.assertRaises(
        function() { prefs.set("test_set_pref", value); },
        ("can't set pref test_set_pref to value '" + value + "'; " +
         "it isn't a string, integer, or boolean"),
        "Setting a pref to " + uneval(value) + " should raise error"
      );
    });
};

exports.testPrefClass = function(test) {
  var branch = Branch("test_foo");

  test.assertEqual(branch.test, undefined, "test_foo.test is undefined");
  branch.test = true;
  test.assertEqual(branch.test, true, "test_foo.test is true");
  delete branch.test;
  test.assertEqual(branch.test, undefined, "test_foo.test is undefined");
};

exports.testGetSetLocalized = function(test) {
  let prefName = "general.useragent.locale";

  // Ensure that "general.useragent.locale" is a 'localized' pref
  let bundleURL = "chrome://global/locale/intl.properties";
  prefs.setLocalized(prefName, bundleURL);

  // Fetch the expected value directly from the property file
  let expectedValue = BundleService.createBundle(bundleURL).
    GetStringFromName(prefName).
    toLowerCase();

  test.assertEqual(prefs.getLocalized(prefName).toLowerCase(),
                   expectedValue,
                   "get localized preference");

  // Undo our modification
  prefs.reset(prefName);
}

// TEST: setting and getting preferences with special characters work
exports.testSpecialChars = function(test) {
  let chars = specialChars.split('');
  const ROOT = "test.";

  chars.forEach(function(char) {
    let rand = Math.random() + "";
    prefs.set(ROOT+char, rand);
    test.assertEqual(prefs.get(ROOT+char), rand, "setting pref with a name that is a special char, " + char + ", worked!");
  });
};
