function postAll(data) {
  self.clients.matchAll().then(function(clients) {
    if (!clients.length) {
      dump(
        "***************** NO CLIENTS FOUND! Test messages are being lost *******************\n"
      );
    }
    clients.forEach(function(client) {
      client.postMessage(data);
    });
  });
}

function ok(a, msg) {
  postAll({ type: "status", status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  postAll({
    type: "status",
    status: a === b,
    msg: a + " === " + b + ": " + msg,
  });
}

function done() {
  postAll({ type: "finish" });
}

onmessage = function(e) {
  dump("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% MESSAGE " + e.data + "\n");
  var start;
  if (e.data == "create") {
    start = registration.showNotification("This is a title");
  } else {
    start = Promise.resolve();
  }

  start.then(function() {
    dump("CALLING getNotification\n");
    registration.getNotifications().then(function(notifications) {
      dump("RECD getNotification\n");
      is(notifications.length, 1, "There should be one stored notification");
      var notification = notifications[0];
      if (!notification) {
        done();
        return;
      }
      ok(notification instanceof Notification, "Should be a Notification");
      is(notification.title, "This is a title", "Title should match");
      notification.close();
      done();
    });
  });
};
