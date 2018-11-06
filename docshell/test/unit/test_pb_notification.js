function destroy_transient_docshell() {
  let windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  windowlessBrowser.docshell.setOriginAttributes({privateBrowsingId : 1});
  windowlessBrowser.close();
  do_test_pending();
  do_timeout(0, Cu.forceGC);
}

function run_test() {
  var obs = {
    observe: function(aSubject, aTopic, aData) {
      Assert.equal(aTopic, "last-pb-context-exited");
      do_test_finished();
    }
  };
  var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  os.addObserver(obs, "last-pb-context-exited");
  destroy_transient_docshell();
}
