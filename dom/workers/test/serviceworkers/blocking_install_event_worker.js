function postMessageToTest(msg) {
  return clients.matchAll({ includeUncontrolled: true })
    .then(list => {
      for (var client of list) {
        if (client.url.endsWith('test_install_event_gc.html')) {
          client.postMessage(msg);
          break;
        }
      }
    });
}

addEventListener('install', evt => {
  // This must be a simple promise to trigger the CC failure.
  evt.waitUntil(new Promise(function() { }));
  postMessageToTest({ type: 'INSTALL_EVENT' });
});
