var url = 'index.html';
self.addEventListener('message', (message) => {
  caches.open('acache').then((cache) => {
    if(message.data == 'write') {
      cache.add(url).then(() => {
        message.source.postMessage({
          type: 'written'
        });
      });
    } else if (message.data == 'read') {
      cache.match(url).then((result) => {
        message.source.postMessage({
          type: 'done',
          cached: !!result
        });
      });
    }
  });
});
