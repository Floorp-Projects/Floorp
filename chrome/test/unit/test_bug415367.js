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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *      Ehsan Akhgari <ehsan.akhgari@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
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
 * ***** END LICENSE BLOCK *****
 */

var gIOS = Cc["@mozilla.org/network/io-service;1"]
            .getService(Ci.nsIIOService);

function test_uri(obj)
{
  var uri = null;
  var failed = false;
  var message = "";
  try {
    uri = gIOS.newURI(obj.uri, null, null);
    if (!obj.result) {
      failed = true;
      message = obj.uri + " should not be accepted as a valid URI";
    }
  }
  catch (ex) {
    if (obj.result) {
      failed = true;
      message = obj.uri + " should be accepted as a valid URI";
    }
  }
  if (failed)
    do_throw(message);
  if (obj.result) {
    do_check_true(uri != null);
    do_check_eq(uri.spec, obj.uri);
  }
}

function run_test()
{
  var tests = [
    {uri: "chrome://blah/content/blah.xul", result: true},
    {uri: "chrome://blah/content/:/blah/blah.xul", result: false},
    {uri: "chrome://blah/content/%252e./blah/blah.xul", result: false},
    {uri: "chrome://blah/content/%252e%252e/blah/blah.xul", result: false},
    {uri: "chrome://blah/content/blah.xul?param=%252e./blah/", result: true},
    {uri: "chrome://blah/content/blah.xul?param=:/blah/", result: true},
    {uri: "chrome://blah/content/blah.xul?param=%252e%252e/blah/", result: true},
  ];
  for (var i = 0; i < tests.length; ++ i)
    test_uri(tests[i]);
}
