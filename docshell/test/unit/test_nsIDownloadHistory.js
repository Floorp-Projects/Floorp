/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  var testURI = ios.newURI("http://google.com/");

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
  os.addObserver(obs, NS_LINK_VISITED_EVENT_TOPIC);

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

  // Needed to properly setup and shutdown the profile.
  do_get_profile();

  for (var i = 0; i < tests.length; i++)
    tests[i]();

  cleanup();
}
