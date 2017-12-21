var Cc = Components.classes;
var Ci = Components.interfaces;

function run_test() {
  var notifications = 0;
  var obs = {
    observe: function(aSubject, aTopic, aData) {
      Assert.equal(aTopic, "last-pb-context-exited");
      notifications++;
    }
  };
  var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  os.addObserver(obs, "last-pb-context-exited");
 
  run_test_in_child("../unit/test_pb_notification.js",
                    function() {
                      Assert.equal(notifications, 1);
                      do_test_finished();
                    });
}