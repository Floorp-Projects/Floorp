function run_test() {
  var notifications = 0;
  var obs = {
    observe(aSubject, aTopic, aData) {
      Assert.equal(aTopic, "last-pb-context-exited");
      notifications++;
    },
  };
  Services.obs.addObserver(obs, "last-pb-context-exited");

  run_test_in_child("../unit/test_pb_notification.js", function() {
    Assert.equal(notifications, 1);
    do_test_finished();
  });
}
