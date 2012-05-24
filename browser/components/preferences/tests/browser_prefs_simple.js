function test() {
  waitForExplicitFinish();
  let connectionURI = "chrome://browser/content/preferences/connection.xul";
  let preferencesURI = "chrome://browser/content/preferences/preferences.xul";

  //open pref window, open connection subdialog, accept subdialog, accept pref window

  let observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        win.addEventListener("load", function() {
          win.removeEventListener("load", arguments.callee, false);
          if (win.location.href == preferencesURI) {
            ok(true, "preferences window opened");
            win.addEventListener("DOMModalDialogClosed", function() {
              ok(true, "connection window closed");
              win.document.documentElement.acceptDialog();
            });
            win.document.documentElement.openSubDialog(connectionURI, "", null);
          }
          else if (win.location.href == connectionURI) {
            ok(true, "connection window opened");
            win.document.getElementById("network.proxy.no_proxies_on").value = "blah";
            win.document.documentElement.acceptDialog();
          }
        }, false);
      }
      else if (aTopic == "domwindowclosed") {
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        if (win.location.href == preferencesURI) {
          windowWatcher.unregisterNotification(observer);
          ok(true, "preferences window closed");
          is(Services.prefs.getCharPref("network.proxy.no_proxies_on"), "blah", "saved pref");
          finish();
        }
      }
    }
  }
  
  var windowWatcher = Cc["@mozilla.org/embedcomp/window-watcher;1"]
                        .getService(Components.interfaces.nsIWindowWatcher);
  windowWatcher.registerNotification(observer);
  
  openPreferences();

}
