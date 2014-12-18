function is(a, b, msg) {
  postMessage((a == b ? 'OK' : 'KO')+ ' ' + msg)
}

var store;
navigator.getDataStores('foo').then(function(stores) {
  is(stores.length, 1, "getDataStores('foo') returns 1 element");
  is(stores[0].name, 'foo', 'The dataStore.name is foo');
  is(stores[0].readOnly, false, 'The dataStore foo is not in readonly');
  store = stores[0];
  postMessage('DONE');
});

onclose = function() {
  for (var i = 0; i < 100; ++i) {
    store.get(123);
  }
}
