"use strict";

const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const {OS} = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const fooNotification =
  getNotificationObject("foo", "a4f1d54a-98b7-4231-9120-5afc26545bad", null, true);
const barNotification =
  getNotificationObject("bar", "a4f1d54a-98b7-4231-9120-5afc26545bad", "baz", true);
const msg = "Notification:GetAll";
const msgReply = "Notification:GetAll:Return:OK";

do_get_profile();
const OLD_STORE_PATH =
    OS.Path.join(OS.Constants.Path.profileDir, "notificationstore.json");

let nextRequestID = 0;

// Create the old datastore and populate it with data before we initialize
// the notification database so it has data to migrate.  This is a setup step,
// not a test, but it seems like we need to do it in a test function
// rather than in run_test() because the test runner doesn't handle async steps
// in run_test().
add_task({
  skip_if: () => !AppConstants.MOZ_NEW_NOTIFICATION_STORE,
}, async function test_create_old_datastore() {
  const notifications = {
    [fooNotification.origin]: {
      [fooNotification.id]: fooNotification,
    },
    [barNotification.origin]: {
      [barNotification.id]: barNotification,
    },
  };

  await OS.File.writeAtomic(OLD_STORE_PATH, JSON.stringify(notifications));

  startNotificationDB();
});

add_test({
  skip_if: () => !AppConstants.MOZ_NEW_NOTIFICATION_STORE,
}, function test_get_system_notification() {
  const requestID = nextRequestID++;
  const msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
    Assert.equal(0, message.data.notifications.length);
  };

  addAndSend(msg, msgReply, msgHandler, {
    origin: systemNotification.origin,
    requestID,
  });
});

add_test({
  skip_if: () => !AppConstants.MOZ_NEW_NOTIFICATION_STORE,
}, function test_get_foo_notification() {
  const requestID = nextRequestID++;
  const msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
    Assert.equal(1, message.data.notifications.length);
    Assert.deepEqual(fooNotification, message.data.notifications[0],
      "Notification data migrated");
  };

  addAndSend(msg, msgReply, msgHandler, {
    origin: fooNotification.origin,
    requestID,
  });
});

add_test({
  skip_if: () => !AppConstants.MOZ_NEW_NOTIFICATION_STORE,
}, function test_get_bar_notification() {
  const requestID = nextRequestID++;
  const msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
    Assert.equal(1, message.data.notifications.length);
    Assert.deepEqual(barNotification, message.data.notifications[0],
      "Notification data migrated");
  };

  addAndSend(msg, msgReply, msgHandler, {
    origin: barNotification.origin,
    requestID,
  });
});

add_task({
  skip_if: () => !AppConstants.MOZ_NEW_NOTIFICATION_STORE,
}, async function test_old_datastore_deleted() {
    Assert.ok(!await OS.File.exists(OLD_STORE_PATH),
      "old datastore no longer exists");
});
