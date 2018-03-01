"use strict";

function run_test() {
  do_get_profile();
  startNotificationDB();
  run_next_test();
}

// Get one notification, none exists
add_test(function test_get_none() {
  let requestID = 0;
  let msgReply = "Notification:GetAll:Return:OK";
  let msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
    Assert.equal(0, message.data.notifications.length);
  };

  addAndSend("Notification:GetAll", msgReply, msgHandler, {
    origin: systemNotification.origin,
    requestID: requestID
  });
});

// Store one notification
add_test(function test_send_one() {
  let requestID = 1;
  let msgReply = "Notification:Save:Return:OK";
  let msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
  };

  addAndSend("Notification:Save", msgReply, msgHandler, {
    origin: systemNotification.origin,
    notification: systemNotification,
    requestID: requestID
  });
});

// Get one notification, one exists
add_test(function test_get_one() {
  let requestID = 2;
  let msgReply = "Notification:GetAll:Return:OK";
  let msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
    Assert.equal(1, message.data.notifications.length);
    // compare the content
    compareNotification(systemNotification, message.data.notifications[0]);
  };

  addAndSend("Notification:GetAll", msgReply, msgHandler, {
    origin: systemNotification.origin,
    requestID: requestID
  });
});

// Delete one notification
add_test(function test_delete_one() {
  let requestID = 3;
  let msgReply = "Notification:Delete:Return:OK";
  let msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
  };

  addAndSend("Notification:Delete", msgReply, msgHandler, {
    origin: systemNotification.origin,
    id: systemNotification.id,
    requestID: requestID
  });
});

// Get one notification, none exists
add_test(function test_get_none_again() {
  let requestID = 4;
  let msgReply = "Notification:GetAll:Return:OK";
  let msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
    Assert.equal(0, message.data.notifications.length);
  };

  addAndSend("Notification:GetAll", msgReply, msgHandler, {
    origin: systemNotification.origin,
    requestID: requestID
  });
});

// Delete one notification that do not exists anymore
add_test(function test_delete_one_nonexistent() {
  let requestID = 5;
  let msgReply = "Notification:Delete:Return:OK";
  let msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
  };

  addAndSend("Notification:Delete", msgReply, msgHandler, {
    origin: systemNotification.origin,
    id: systemNotification.id,
    requestID: requestID
  });
});

// Store two notifications with the same id
add_test(function test_send_two_get_one() {
  let requestID = 6;
  let calls = 0;

  let msgGetReply = "Notification:GetAll:Return:OK";
  let msgGetHandler = function(message) {
    Assert.equal(requestID + 2, message.data.requestID);
    Assert.equal(1, message.data.notifications.length);
    // compare the content
    compareNotification(systemNotification, message.data.notifications[0]);
  };

  let msgSaveReply = "Notification:Save:Return:OK";
  let msgSaveHandler = function(message) {
    calls += 1;
    if (calls === 2) {
      addAndSend("Notification:GetAll", msgGetReply, msgGetHandler, {
        origin: systemNotification.origin,
        requestID: (requestID + 2)
      });
    }
  };

  addAndSend("Notification:Save", msgSaveReply, msgSaveHandler, {
    origin: systemNotification.origin,
    notification: systemNotification,
    requestID: requestID
  }, false);

  addAndSend("Notification:Save", msgSaveReply, msgSaveHandler, {
    origin: systemNotification.origin,
    notification: systemNotification,
    requestID: (requestID + 1)
  }, false);
});

// Delete previous notification
add_test(function test_delete_previous() {
  let requestID = 8;
  let msgReply = "Notification:Delete:Return:OK";
  let msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
  };

  addAndSend("Notification:Delete", msgReply, msgHandler, {
    origin: systemNotification.origin,
    id: systemNotification.id,
    requestID: requestID
  });
});

// Store two notifications from same origin with the same tag
add_test(function test_send_two_get_one() {
  let requestID = 10;
  let tag = "voicemail";

  let systemNotification1 =
    getNotificationObject("system", "{f271f9ee-3955-4c10-b1f2-af552fb270ee}", tag);
  let systemNotification2 =
    getNotificationObject("system", "{8ef9a628-f0f4-44b4-820d-c117573c33e3}", tag);

  let msgGetReply = "Notification:GetAll:Return:OK";
  let msgGetNotifHandler = {
    receiveMessage: function(message) {
      if (message.name === msgGetReply) {
        cpmm.removeMessageListener(msgGetReply, msgGetNotifHandler);
        let notifications = message.data.notifications;
        // same tag, so replaced
        Assert.equal(1, notifications.length);
        // compare the content
        compareNotification(systemNotification2, notifications[0]);
        run_next_test();
      }
    }
  };

  cpmm.addMessageListener(msgGetReply, msgGetNotifHandler);

  let msgSaveReply = "Notification:Save:Return:OK";
  let msgSaveCalls = 0;
  let msgSaveHandler = function(message) {
    msgSaveCalls++;
    // Once both request have been sent, trigger getall
    if (msgSaveCalls === 2) {
      cpmm.sendAsyncMessage("Notification:GetAll", {
        origin: systemNotification1.origin,
        requestID: message.data.requestID + 2 // 12, 13
      });
    }
  };

  addAndSend("Notification:Save", msgSaveReply, msgSaveHandler, {
    origin: systemNotification1.origin,
    notification: systemNotification1,
    requestID: requestID // 10
  }, false);

  addAndSend("Notification:Save", msgSaveReply, msgSaveHandler, {
    origin: systemNotification2.origin,
    notification: systemNotification2,
    requestID: (requestID + 1) // 11
  }, false);
});

// Delete previous notification
add_test(function test_delete_previous() {
  let requestID = 15;
  let msgReply = "Notification:Delete:Return:OK";
  let msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
  };

  addAndSend("Notification:Delete", msgReply, msgHandler, {
    origin: systemNotification.origin,
    id: "{8ef9a628-f0f4-44b4-820d-c117573c33e3}",
    requestID: requestID
  });
});

// Store two notifications from two origins with the same tag
add_test(function test_send_two_get_two() {
  let requestID = 20;
  let tag = "voicemail";

  let systemNotification1 = systemNotification;
  systemNotification1.tag = tag;

  let calendarNotification2 = calendarNotification;
  calendarNotification2.tag = tag;

  let msgGetReply = "Notification:GetAll:Return:OK";
  let msgGetCalls = 0;
  let msgGetHandler = {
    receiveMessage: function(message) {
      if (message.name === msgGetReply) {
        msgGetCalls++;
        let notifications = message.data.notifications;

        // one notification per origin
        Assert.equal(1, notifications.length);

        // first call should be system notification
        if (msgGetCalls === 1) {
          compareNotification(systemNotification1, notifications[0]);
        }

        // second and last call should be calendar notification
        if (msgGetCalls === 2) {
          cpmm.removeMessageListener(msgGetReply, msgGetHandler);
          compareNotification(calendarNotification2, notifications[0]);
          run_next_test();
        }
      }
    }
  };
  cpmm.addMessageListener(msgGetReply, msgGetHandler);

  let msgSaveReply = "Notification:Save:Return:OK";
  let msgSaveCalls = 0;
  let msgSaveHandler = {
    receiveMessage: function(message) {
      if (message.name === msgSaveReply) {
        msgSaveCalls++;
        if (msgSaveCalls === 2) {
          cpmm.removeMessageListener(msgSaveReply, msgSaveHandler);

          // Trigger getall for each origin
          cpmm.sendAsyncMessage("Notification:GetAll", {
            origin: systemNotification1.origin,
            requestID: message.data.requestID + 1 // 22
          });

          cpmm.sendAsyncMessage("Notification:GetAll", {
            origin: calendarNotification2.origin,
            requestID: message.data.requestID + 2 // 23
          });
        }
      }
    }
  };
  cpmm.addMessageListener(msgSaveReply, msgSaveHandler);

  cpmm.sendAsyncMessage("Notification:Save", {
    origin: systemNotification1.origin,
    notification: systemNotification1,
    requestID: requestID // 20
  });

  cpmm.sendAsyncMessage("Notification:Save", {
    origin: calendarNotification2.origin,
    notification: calendarNotification2,
    requestID: (requestID + 1) // 21
  });
});

// Cleanup previous notification
add_test(function test_delete_previous() {
  let requestID = 25;
  let msgReply = "Notification:Delete:Return:OK";
  let msgHandler = function(message) {
    Assert.equal(requestID, message.data.requestID);
  };

  addAndSend("Notification:Delete", msgReply, msgHandler, {
    origin: systemNotification.origin,
    id: "{2bc883bf-2809-4432-b0f4-f54e10372764}",
    requestID: requestID
  });
});
