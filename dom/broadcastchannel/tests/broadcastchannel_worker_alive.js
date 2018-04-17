(new BroadcastChannel('foobar')).addEventListener('message', function(event) {
  if (event.data != 'READY') {
    event.target.postMessage(event.data);
  }
});

(new BroadcastChannel('foobar')).postMessage('READY');
