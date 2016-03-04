/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

function run_test() {
  do_get_profile();
  run_next_test();
}

add_task(function* test_service_parent() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});
  yield setUpServiceInParent(PushService, db);

  // Accessing the lazy service getter will start the service in the main
  // process.
  equal(PushServiceComponent.pushTopic, "push-message",
    "Wrong push message observer topic");
  equal(PushServiceComponent.subscriptionChangeTopic,
    "push-subscription-change", "Wrong subscription change observer topic");

  yield run_test_in_child('./test_service_child.js');

  yield tearDownServiceInParent(db);
});
