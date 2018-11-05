var gNotifications = 0;

ChromeUtils.import("resource://gre/modules/Services.jsm");

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
  let windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  windowlessBrowser.docShell.addWeakPrivacyTransitionObserver(observer);
  windowlessBrowser.docShell.setOriginAttributes({privateBrowsingId : 1});
  windowlessBrowser.docShell.setOriginAttributes({privateBrowsingId : 0});
  windowlessBrowser.close();
  Assert.equal(gNotifications, 2);
}
