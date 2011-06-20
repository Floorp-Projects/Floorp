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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Dave Townsend <dtownsend@oxymoronical.com> (original author)
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
 
function run_test() {
  var scope1 = {};
  var global1 = Components.utils.import("resource://gre/modules/NetUtil.jsm", scope1);

  var scope2 = {};
  var global2 = Components.utils.import("resource://gre/modules/NetUtil.jsm", scope2);

  do_check_true(global1 === global2);
  do_check_true(scope1.NetUtil === scope2.NetUtil);

  Components.utils.unload("resource://gre/modules/NetUtil.jsm");

  var scope3 = {};
  var global3 = Components.utils.import("resource://gre/modules/NetUtil.jsm", scope3);

  do_check_false(global1 === global3);
  do_check_false(scope1.NetUtil === scope3.NetUtil);

  // Both instances should work
  uri1 = scope1.NetUtil.newURI("http://www.example.com");
  do_check_true(uri1 instanceof Components.interfaces.nsIURL);

  var uri3 = scope3.NetUtil.newURI("http://www.example.com");
  do_check_true(uri3 instanceof Components.interfaces.nsIURL);

  do_check_true(uri1.equals(uri3));
}
