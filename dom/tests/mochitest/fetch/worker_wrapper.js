importScripts("utils.js");

function getScriptUrl() {
  return new URL(location.href).searchParams.get("script");
}

importScripts(getScriptUrl());

var client;
var context;

function ok(a, msg) {
  client.postMessage({
    type: "status",
    status: !!a,
    msg: a + ": " + msg,
    context,
  });
}

function is(a, b, msg) {
  client.postMessage({
    type: "status",
    status: a === b,
    msg: a + " === " + b + ": " + msg,
    context,
  });
}

addEventListener("message", function workerWrapperOnMessage(e) {
  removeEventListener("message", workerWrapperOnMessage);
  var data = e.data;

  function runTestAndReportToClient(event) {
    var done = function() {
      client.postMessage({ type: "finish", context });
    };

    try {
      // runTest() is provided by the test.
      var promise = runTest().finally(done);
      if ("waitUntil" in event) {
        event.waitUntil(promise);
      }
    } catch (e) {
      client.postMessage({
        type: "status",
        status: false,
        msg: "worker failed to run " + data.script + "; error: " + e.message,
        context,
      });
      done();
    }
  }

  if ("ServiceWorker" in self) {
    // Fetch requests from a service worker are not intercepted.
    self.isSWPresent = false;

    e.waitUntil(
      self.clients
        .matchAll({ includeUncontrolled: true })
        .then(function(clients) {
          for (var i = 0; i < clients.length; ++i) {
            if (clients[i].url.indexOf("message_receiver.html") > -1) {
              client = clients[i];
              break;
            }
          }
          if (!client) {
            dump(
              "We couldn't find the message_receiver window, the test will fail\n"
            );
          }
          context = "ServiceWorker";
          runTestAndReportToClient(e);
        })
    );
  } else {
    client = self;
    context = "Worker";
    runTestAndReportToClient(e);
  }
});
