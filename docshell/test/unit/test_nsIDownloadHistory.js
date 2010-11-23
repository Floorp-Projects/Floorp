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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

const NS_DOWNLOADHISTORY_CID = "{2ee83680-2af0-4bcb-bfa0-c9705f6554f1}";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "Services", function() {
  Components.utils.import("resource://gre/modules/Services.jsm");
  return Services;
});

function testLinkVistedObserver()
{
  const NS_LINK_VISITED_EVENT_TOPIC = "link-visited";
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var testURI = ios.newURI("http://google.com/", null, null);

  var gh = Cc["@mozilla.org/browser/global-history;2"].
           getService(Ci.nsIGlobalHistory2);
  do_check_false(gh.isVisited(testURI));

  var topicReceived = false;
  var obs = {
    observe: function tlvo_observe(aSubject, aTopic, aData)
    {
      if (NS_LINK_VISITED_EVENT_TOPIC == aTopic) {
        do_check_eq(testURI, aSubject);
        topicReceived = true;
      }
    }
  };

  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(obs, NS_LINK_VISITED_EVENT_TOPIC, false);

  var dh = Components.classesByID[NS_DOWNLOADHISTORY_CID].
           getService(Ci.nsIDownloadHistory);
  dh.addDownload(testURI);
  do_check_true(topicReceived);
  do_check_true(gh.isVisited(testURI));
}

var tests = [testLinkVistedObserver];

function run_test()
{
  // Not everyone uses/defines an nsGlobalHistory* service. Especially if
  // MOZ_PLACES is not defined. If getService fails, then abort gracefully.
  try {
    Cc["@mozilla.org/browser/global-history;2"].
      getService(Ci.nsIGlobalHistory2);
  }
  catch (ex) {
    return;
  }

  Services.prefs.setBoolPref("places.history.enabled", true);

  for (var i = 0; i < tests.length; i++)
    tests[i]();

  cleanup();
}
