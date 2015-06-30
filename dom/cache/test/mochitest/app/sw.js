self.addEventListener('message', (message) => {
  caches.open('acache').then((cache) => {
    if(message.data == 'write') {
      cache.add('aurl').then(() => {
        message.source.postMessage({
          type: 'written'
        });
      });
    } else if (message.data == 'read') {
      cache.match('aurl').then((result) => {
        message.source.postMessage({
          type: 'done',
          cached: !!result
        });
      });
    }
  });
});
