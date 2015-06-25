function ok(test, message) {
  postMessage({ type: 'ok', test: test, message: message });
}

function is(a, b, message) {
  postMessage({ type: 'is', test1: a, test2: b, message: message });
}

if (self.Notification) {
  var child = new Worker("notification_worker_child-child.js");
  child.onerror = function(e) {
    ok(false, "Error loading child worker " + e);
    postMessage({ type: 'finish' });
  }

  child.onmessage = function(e) {
    postMessage(e.data);
  }

  onmessage = function(e) {
    child.postMessage('start');
  }
} else {
  ok(true, "Notifications are not enabled in workers on the platform.");
  postMessage({ type: 'finish' });
}
