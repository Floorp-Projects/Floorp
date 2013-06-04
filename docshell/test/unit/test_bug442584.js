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
    var uri = ios.newURI("http://localhost/" + i, null, null);
    prefetch.prefetchURI(uri, uri, null, true);
  }

  // Make sure the queue has items in it...
  var queue = prefetch.enumerateQueue();
  do_check_true(queue.hasMoreElements());

  // Now disable the pref to force the queue to empty...
  prefs.setBoolPref("network.prefetch-next", false);
  queue = prefetch.enumerateQueue();
  do_check_false(queue.hasMoreElements());

  // Now reenable the pref, and add more items to the queue.
  prefs.setBoolPref("network.prefetch-next", true);
  for (var i = 0; i < 5; i++) {
    var uri = ios.newURI("http://localhost/" + i, null, null);
    prefetch.prefetchURI(uri, uri, null, true);
  }
  queue = prefetch.enumerateQueue();
  do_check_true(queue.hasMoreElements());
}

