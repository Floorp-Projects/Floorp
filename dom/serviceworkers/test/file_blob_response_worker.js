function makeFileBlob(obj) {
  return new Promise(function(resolve, reject) {
    var request = indexedDB.open('file_blob_response_worker', 1);
    request.onerror = reject;
    request.onupgradeneeded = function(evt) {
      var db = evt.target.result;
      db.onerror = reject;

      var objectStore = db.createObjectStore('test', { autoIncrement: true });
      var index = objectStore.createIndex('test', 'index');
    };

    request.onsuccess = function(evt) {
      var db = evt.target.result;
      db.onerror = reject;

      var blob = new Blob([JSON.stringify(obj)],
                          { type: 'application/json' });
      var data = { blob: blob, index: 5 };

      objectStore = db.transaction('test', 'readwrite').objectStore('test');
      objectStore.add(data).onsuccess = function(event) {
        var key = event.target.result;
        objectStore = db.transaction('test').objectStore('test');
        objectStore.get(key).onsuccess = function(event1) {
          resolve(event1.target.result.blob);
        };
      };
    };
  });
}

self.addEventListener('fetch', function(evt) {
  var result = { value: 'success' };
  evt.respondWith(makeFileBlob(result).then(function(blob) {
    return new Response(blob)
  }));
});
