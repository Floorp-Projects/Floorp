"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

// Test that a reset profile dialog appears when "resetFirefox" event is triggered
add_UITour_task(function* test_resetFirefox() {
  let dialogPromise = new Promise((resolve) => {
    let winWatcher = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                     getService(Ci.nsIWindowWatcher);
    winWatcher.registerNotification(function onOpen(subj, topic, data) {
      if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
        subj.addEventListener("load", function onLoad() {
          subj.removeEventListener("load", onLoad);
          if (subj.document.documentURI ==
              "chrome://global/content/resetProfile.xul") {
            winWatcher.unregisterNotification(onOpen);
            ok(true, "Observed search manager window open");
            is(subj.opener, window,
               "Reset Firefox event opened a reset profile window.");
            subj.close();
            resolve();
          }
        });
      }
    });
  });
  yield gContentAPI.resetFirefox();
  yield dialogPromise;
});

