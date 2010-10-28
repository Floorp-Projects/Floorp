// Make sure that we clean up after each test if it times out, for example.
registerCleanupFunction(function() {
  var pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  ok(!pb.privateBrowsingEnabled, "Private browsing should be terminated after finishing the test");
  pb.privateBrowsingEnabled = false;
  try {
    Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
  } catch(e) {}
});

