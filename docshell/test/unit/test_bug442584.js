var prefetch = Cc["@mozilla.org/prefetch-service;1"].getService(
  Ci.nsIPrefetchService
);

var ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

function run_test() {
  // Fill up the queue
  Services.prefs.setBoolPref("network.prefetch-next", true);
  for (var i = 0; i < 5; i++) {
    var uri = Services.io.newURI("http://localhost/" + i);
    var referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, uri);
    prefetch.prefetchURI(uri, referrerInfo, null, true);
  }

  // Make sure the queue has items in it...
  Assert.ok(prefetch.hasMoreElements());

  // Now disable the pref to force the queue to empty...
  Services.prefs.setBoolPref("network.prefetch-next", false);
  Assert.ok(!prefetch.hasMoreElements());

  // Now reenable the pref, and add more items to the queue.
  Services.prefs.setBoolPref("network.prefetch-next", true);
  for (var k = 0; k < 5; k++) {
    var uri2 = Services.io.newURI("http://localhost/" + k);
    var referrerInfo2 = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, uri2);
    prefetch.prefetchURI(uri2, referrerInfo2, null, true);
  }
  Assert.ok(prefetch.hasMoreElements());
}
