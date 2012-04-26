if (typeof Cc === "undefined")
  Cc = Components.classes;
if (typeof Ci === "undefined")
  Ci = Components.interfaces;

function destroy_transient_docshell() {
  var docshell = Cc["@mozilla.org/docshell;1"].createInstance(Ci.nsIDocShell);
  docshell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing = true;
  do_test_pending();
  do_timeout(0, Components.utils.forceGC);
}

function run_test() {
  var obs = {
    observe: function(aSubject, aTopic, aData) {
      do_check_eq(aTopic, "last-pb-context-exited");
      do_test_finished();
    }
  };
  var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  os.addObserver(obs, "last-pb-context-exited", false);
  destroy_transient_docshell();
}