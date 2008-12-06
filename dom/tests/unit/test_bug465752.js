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
 *      Boris Zbarsky <bzbarsky@mit.edu>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
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

function run_test()
{
  const ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
  const str = "javascript:10";
  var uri = ios.newURI(str, null, null);
  var uri2 = ios.newURI(str, null, null);
  const str2 = "http://example.org";
  var uri3 = ios.newURI(str2, null, null);
  do_check_true(uri.equals(uri));
  do_check_true(uri.equals(uri2));
  do_check_true(uri2.equals(uri));
  do_check_true(uri2.equals(uri2));
  do_check_false(uri3.equals(uri2));
  do_check_false(uri2.equals(uri3));

  var simple = Components.classes["@mozilla.org/network/simple-uri;1"]
                         .createInstance(Components.interfaces.nsIURI);
  simple.spec = str;
  do_check_eq(simple.spec, uri.spec);
  do_check_false(simple.equals(uri));
  do_check_false(uri.equals(simple));
}
