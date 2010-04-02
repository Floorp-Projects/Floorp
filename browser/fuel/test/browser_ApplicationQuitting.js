function test() {
  function quitRequestObserver(aSubject, aTopic, aData) {
    ok(aTopic == "quit-application-requested" &&
       aSubject instanceof Components.interfaces.nsISupportsPRBool,
       "Received a quit request we're going to deny");
    aSubject.data = true;
  }

  // ensure that we don't accidentally quit
  Services.obs.addObserver(quitRequestObserver, "quit-application-requested", false);

  ok(!Application.quit(),    "Tried to quit - and didn't succeed");
  ok(!Application.restart(), "Tried to restart - and didn't succeed");

  // clean up
  Services.obs.removeObserver(quitRequestObserver, "quit-application-requested", false);
}
