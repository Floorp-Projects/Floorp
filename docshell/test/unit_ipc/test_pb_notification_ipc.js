var Cc = Components.classes;
var Ci = Components.interfaces;

function run_test() {
  var notifications = 0;
  var obs = {
    observe: function(aSubject, aTopic, aData) {
      do_check_eq(aTopic, "last-pb-context-exited");
      notifications++;
    }
  };
  var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  os.addObserver(obs, "last-pb-context-exited", false);
 
  run_test_in_child("../unit/test_pb_notification.js",
                    function() {
                      do_check_eq(notifications, 1);
                      do_test_finished();
                    });
}