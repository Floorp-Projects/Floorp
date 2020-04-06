var gNotifications = 0;

var observer = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIPrivacyTransitionObserver",
    "nsISupportsWeakReference",
  ]),

  privateModeChanged(enabled) {
    gNotifications++;
  },
};

function run_test() {
  let windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  windowlessBrowser.docShell.addWeakPrivacyTransitionObserver(observer);
  windowlessBrowser.docShell.setOriginAttributes({ privateBrowsingId: 1 });
  windowlessBrowser.docShell.setOriginAttributes({ privateBrowsingId: 0 });
  windowlessBrowser.close();
  Assert.equal(gNotifications, 2);
}
