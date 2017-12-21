/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
function test_BrokenFile(path, shouldThrow, expectedName) {
  var didThrow = false;
  try {
    Components.utils.import(path);
  } catch (ex) {
    var exceptionName = ex.name;
    print("ex: " + ex + "; name = " + ex.name);
    didThrow = true;
  }

  Assert.equal(didThrow, shouldThrow);
  if (didThrow)
    Assert.equal(exceptionName, expectedName);
}

function run_test() {
  test_BrokenFile("resource://test/bogus_exports_type.jsm", true, "Error");

  test_BrokenFile("resource://test/bogus_element_type.jsm", true, "Error");

  test_BrokenFile("resource://test/non_existing.jsm",
                  true,
                  "NS_ERROR_FILE_NOT_FOUND");

  test_BrokenFile("chrome://test/content/test.jsm",
                  true,
                  "NS_ERROR_FILE_NOT_FOUND");

  // check that we can access modules' global objects even if
  // EXPORTED_SYMBOLS is missing or ill-formed:
  Assert.equal(typeof(Components.utils.import("resource://test/bogus_exports_type.jsm",
                                              null)),
               "object");

  Assert.equal(typeof(Components.utils.import("resource://test/bogus_element_type.jsm",
                                              null)),
               "object");
}
