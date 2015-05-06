onconnect = function(e) {
  e.ports[0].onmessage = function(e) {
    var request = indexedDB.open(e.data, { version: 1,
                                           storage: "persistent" });
    request.onsuccess = function(event) {
      e.target.postMessage({ status: 'success',
                             isIDBDatabase: (event.target.result instanceof IDBDatabase) });
    }

    request.onerror = function(event) {
      e.target.postMessage({ status: 'error', error: event.target.error.name });
    }
  }
}
