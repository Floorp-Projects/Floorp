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
 * The Original Code is XPConnect Test Code.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bobby Holley <bobbyholley@gmail.com>
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

// Globals
var gInterface = Components.interfaces["nsIXPCTestObjectReadWrite"];

function run_test() {

  // Load the component manifests.
  Components.manager.autoRegister(do_get_file('../components/native/xpctest.manifest'));
  Components.manager.autoRegister(do_get_file('../components/js/xpctest.manifest'));

  // Test for each component.
  test_component("@mozilla.org/js/xpc/test/native/ObjectReadWrite;1");
  test_component("@mozilla.org/js/xpc/test/js/ObjectReadWrite;1");

}

function test_component(contractid) {

  // Instantiate the object.
  var o = Components.classes[contractid].createInstance(gInterface);

  // Test the initial values.
  do_check_eq("XPConnect Read-Writable String", o.stringProperty);
  do_check_eq(true, o.booleanProperty);
  do_check_eq(32767, o.shortProperty);
  do_check_eq(2147483647, o.longProperty);
  do_check_true(5.25 < o.floatProperty && 5.75 > o.floatProperty);
  do_check_eq("X", o.charProperty);

  // Write new values.
  o.stringProperty = "another string";
  o.booleanProperty = false;
  o.shortProperty = -12345;
  o.longProperty = 1234567890;
  o.floatProperty = 10.2;
  o.charProperty = "Z";

  // Test the new values.
  do_check_eq("another string", o.stringProperty);
  do_check_eq(false, o.booleanProperty);
  do_check_eq(-12345, o.shortProperty);
  do_check_eq(1234567890, o.longProperty);
  do_check_true(10.15 < o.floatProperty && 10.25 > o.floatProperty);
  do_check_eq("Z", o.charProperty);

  // Assign values that differ from the expected type to verify conversion.

  function SetAndTestBooleanProperty(newValue, expectedValue) {
    o.booleanProperty = newValue;
    do_check_eq(expectedValue, o.booleanProperty);
  };
  SetAndTestBooleanProperty(false, false);
  SetAndTestBooleanProperty(1, true);
  SetAndTestBooleanProperty(null, false);
  SetAndTestBooleanProperty("A", true);
  SetAndTestBooleanProperty(undefined, false);
  SetAndTestBooleanProperty([], true);
  SetAndTestBooleanProperty({}, true);
}

