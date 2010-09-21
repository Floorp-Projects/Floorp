/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 **/

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let searchText = "test search";

  let listener = {
    onStateChange: function onLocationChange(webProgress, req, flags, status) {
      ok(flags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT, "only notified for document");

      // Only care about starts
      if (!(flags & Ci.nsIWebProgressListener.STATE_START))
        return;

      ok(req instanceof Ci.nsIChannel, "req is a channel");

      let searchURL = Services.search.originalDefaultEngine.getSubmission(searchText).uri.spec;
      is(req.originalURI.spec, searchURL, "search URL was loaded");
      info("Actual URI: " + req.URI.spec);

      Services.ww.unregisterNotification(observer);
      gBrowser.removeProgressListener(this);
      executeSoon(function () {
        gBrowser.removeTab(tab);
        finish();
      });
    }
  }
  gBrowser.addProgressListener(listener, Ci.nsIWebProgressListener.NOTIFY_STATE_DOCUMENT);

  let observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        ok(false, "Alert window opened");
        let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
        win.addEventListener("load", function() {
          win.removeEventListener("load", arguments.callee, false);
          win.close();
        }, false);
        gBrowser.removeProgressListener(listener);
        executeSoon(function () {
          gBrowser.removeTab(tab);
          finish();
        });
      }
      Services.ww.unregisterNotification(this);
    }
  };
  Services.ww.registerNotification(observer);

  // Simulate a user entering search terms
  gURLBar.value = searchText;
  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});
}
