(new BroadcastChannel('foobar')).postMessage('READY');

(new BroadcastChannel('foobar')).addEventListener('message', function(event) {
  if (event.data != 'READY') {
    event.target.postMessage(event.data);
  }
}, false);

