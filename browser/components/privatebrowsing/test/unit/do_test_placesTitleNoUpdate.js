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
 * The Original Code is Private Browsing Test Code.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari <ehsan.akhgari@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
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
 * ***** END LICENSE BLOCK ***** */

// Test to make sure that the visited page titles do not get updated inside the
// private browsing mode.

function do_test()
{
  let pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
           getService(Ci.nsIPrivateBrowsingService);
  let histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  let bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);

  const TEST_URI = uri("http://mozilla.com/privatebrowsing");
  const TITLE_1 = "Title 1";
  const TITLE_2 = "Title 2";

  bhist.removeAllPages();

  bhist.addPageWithDetails(TEST_URI, TITLE_1, Date.now() * 1000);
  do_check_eq(histsvc.getPageTitle(TEST_URI), TITLE_1);

  pb.privateBrowsingEnabled = true;

  bhist.addPageWithDetails(TEST_URI, TITLE_2, Date.now() * 2000);
  do_check_eq(histsvc.getPageTitle(TEST_URI), TITLE_1);

  pb.privateBrowsingEnabled = false;

  do_check_eq(histsvc.getPageTitle(TEST_URI), TITLE_1);

  pb.privateBrowsingEnabled = true;

  bhist.setPageTitle(TEST_URI, TITLE_2);
  do_check_eq(histsvc.getPageTitle(TEST_URI), TITLE_1);

  pb.privateBrowsingEnabled = false;

  do_check_eq(histsvc.getPageTitle(TEST_URI), TITLE_1);

  bhist.removeAllPages();
}

// Support running tests on both the service itself and its wrapper
function run_test() {
  run_test_on_all_services();
}
