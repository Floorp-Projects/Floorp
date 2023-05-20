const gRoot = "http://mochi.test:8888/tests/dom/serviceworkers/test/";
const gTestURL = gRoot + "test_notification_openWindow.html";
const gClientURL = gRoot + "file_notification_openWindow.html";

onmessage = function (event) {
  if (event.data !== "DONE") {
    dump(`ERROR: received unexpected message: ${JSON.stringify(event.data)}\n`);
  }

  event.waitUntil(
    clients.matchAll({ includeUncontrolled: true }).then(cl => {
      for (let client of cl) {
        // The |gClientURL| window closes itself after posting the DONE message,
        // so we don't need to send it anything here.
        if (client.url === gTestURL) {
          client.postMessage("DONE");
        }
      }
    })
  );
};

onnotificationclick = function (event) {
  clients.openWindow(gClientURL);
};
