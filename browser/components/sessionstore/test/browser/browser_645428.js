/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let ss = Cc["@mozilla.org/browser/sessionstore;1"]
         .getService(Ci.nsISessionStore);

let obs = Cc["@mozilla.org/observer-service;1"]
          .getService(Ci.nsIObserverService);

const NOTIFICATION = "sessionstore-browser-state-restored";

function test() {
  waitForExplicitFinish();

  let observer = {
    observe: function (subject, topic, data) {
      if (NOTIFICATION == topic)
        finish();
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                           Ci.nsISupportsWeakReference]),
  };

  obs.addObserver(observer, NOTIFICATION, true);
  registerCleanupFunction(function () {
    obs.removeObserver(observer, NOTIFICATION, true);
  });

  ss.setBrowserState(JSON.stringify({ windows: [] }));
}
