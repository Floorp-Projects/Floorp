function destroy_transient_docshell() {
  let windowlessBrowser = Services.appShell.createWindowlessBrowser(true);
  windowlessBrowser.docShell.setOriginAttributes({ privateBrowsingId: 1 });
  windowlessBrowser.close();
  do_test_pending();
  do_timeout(0, Cu.forceGC);
}

function run_test() {
  var obs = {
    observe(aSubject, aTopic, aData) {
      Assert.equal(aTopic, "last-pb-context-exited");
      do_test_finished();
    },
  };
  Services.obs.addObserver(obs, "last-pb-context-exited");
  destroy_transient_docshell();
}
