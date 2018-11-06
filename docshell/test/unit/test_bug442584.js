var prefetch = Cc["@mozilla.org/prefetch-service;1"].
               getService(Ci.nsIPrefetchService);
var ios = Cc["@mozilla.org/network/io-service;1"].
          getService(Ci.nsIIOService);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);

function run_test() {
  // Fill up the queue
  prefs.setBoolPref("network.prefetch-next", true);
  for (var i = 0; i < 5; i++) {
    var uri = ios.newURI("http://localhost/" + i);
    prefetch.prefetchURI(uri, uri, null, true);
  }

  // Make sure the queue has items in it...
  Assert.ok(prefetch.hasMoreElements());

  // Now disable the pref to force the queue to empty...
  prefs.setBoolPref("network.prefetch-next", false);
  Assert.ok(!prefetch.hasMoreElements());

  // Now reenable the pref, and add more items to the queue.
  prefs.setBoolPref("network.prefetch-next", true);
  for (var i = 0; i < 5; i++) {
    var uri = ios.newURI("http://localhost/" + i);
    prefetch.prefetchURI(uri, uri, null, true);
  }
  Assert.ok(prefetch.hasMoreElements());
}
