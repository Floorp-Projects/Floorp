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

/**
 * Waits for completion of a clear history operation, before
 * proceeding with aCallback.
 *
 * @param aCallback
 *        Function to be called when done.
 */
function waitForClearHistory(aCallback) {
  Services.obs.addObserver(function observeCH(aSubject, aTopic, aData) {
    Services.obs.removeObserver(observeCH, PlacesUtils.TOPIC_EXPIRATION_FINISHED);
    aCallback();
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);
  PlacesUtils.bhistory.removeAllPages();
}

/*
 * Function created to replace the |privateWindow| setter
 */
function setPrivateWindow(aWindow, aEnable) {
  return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIDocShellTreeItem)
                 .treeOwner
                 .QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIXULWindow)
                 .docShell.QueryInterface(Ci.nsILoadContext)
                 .usePrivateBrowsing = aEnable;
} 
