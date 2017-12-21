var gNotifications = 0;

var observer = {
  QueryInterface: function(iid) {
    if (Ci.nsIPrivacyTransitionObserver.equals(iid) ||
        Ci.nsISupportsWeakReference.equals(iid) ||
        Ci.nsISupports.equals(iid))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  
  privateModeChanged: function(enabled) {
    gNotifications++;
  }
}

function run_test() {
  var docshell = Cc["@mozilla.org/docshell;1"].createInstance(Ci.nsIDocShell);
  docshell.addWeakPrivacyTransitionObserver(observer);
  docshell.setOriginAttributes({ privateBrowsingId : 1 });
  docshell.setOriginAttributes({ privateBrowsingId : 0 });
  Assert.equal(gNotifications, 2);
}