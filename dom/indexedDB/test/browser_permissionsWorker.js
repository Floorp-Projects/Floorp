onmessage = function(e) {
  var request = indexedDB.open(e.data, { version: 1,
                                         storage: "persistent" });
  request.onsuccess = function(event) {
    postMessage({ status: 'success',
                  isIDBDatabase: (event.target.result instanceof IDBDatabase) });
  }

  request.onerror = function(event) {
    postMessage({ status: 'error', error: event.target.error.name });
  }
}
