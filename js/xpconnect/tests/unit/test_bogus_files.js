/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Robert Sayre <sayrer@gmail.com> (original author)
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
 
function test_BrokenFile(path, shouldThrow, expectedName) {
  var didThrow = false;
  try {
    Components.utils.import(path);
  } catch (ex) {
    var exceptionName = ex.name;
    print("ex: " + ex + "; name = " + ex.name);
    didThrow = true;
  }

  do_check_eq(didThrow, shouldThrow);
  if (didThrow)
    do_check_eq(exceptionName, expectedName);
}

function run_test() {
  test_BrokenFile("resource://test/bogus_exports_type.jsm", true, "Error");

  test_BrokenFile("resource://test/bogus_element_type.jsm", true, "Error");

  test_BrokenFile("resource://test/non_existing.jsm",
                  true,
                  "NS_ERROR_FILE_NOT_FOUND");

  test_BrokenFile("chrome://test/content/test.jsm",
                  true,
                  "NS_ERROR_ILLEGAL_VALUE");

  // check that we can access modules' global objects even if
  // EXPORTED_SYMBOLS is missing or ill-formed:
  do_check_eq(typeof(Components.utils.import("resource://test/bogus_exports_type.jsm",
                                             null)),
              "object");

  do_check_eq(typeof(Components.utils.import("resource://test/bogus_element_type.jsm",
                                             null)),
              "object");
}
