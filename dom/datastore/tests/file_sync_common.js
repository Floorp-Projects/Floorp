var gStore;
var gRevisions = [];
var gCursor;
var gExpectedEvents = true;

function testGetDataStores() {
  navigator.getDataStores('foo').then(function(stores) {
    is(stores.length, 1, "getDataStores('foo') returns 1 element");

    gStore = stores[0];
    gRevisions.push(gStore.revisionId);

    gStore.onchange = function(aEvent) {
      ok(gExpectedEvents, "Events received!");
      runTest();
    }

    runTest();
  }, cbError);
}

function testBasicInterface() {
  var cursor = gStore.sync();
  ok(cursor, "Cursor is created");
  is(cursor.store, gStore, "Cursor.store is the store");

  ok("next" in cursor, "Cursor.next exists");
  ok("close" in cursor, "Cursor.close exists");

  cursor.close();

  runTest();
}

function testCursor(cursor, steps) {
  if (!steps.length) {
    runTest();
    return;
  }

  var step = steps.shift();
  cursor.next().then(function(data) {
    ok(!!data, "Cursor.next returns data");
    is(data.operation, step.operation, "Waiting for operation: '" + step.operation + "' received '" + data.operation + "'");


    switch (data.operation) {
      case 'clear':
        is (data.id, null, "'clear' operation wants a null id");
        break;

      case 'done':
        is(/[0-9a-zA-Z]{8}-[0-9a-zA-Z]{4}-[0-9a-zA-Z]{4}-[0-9a-zA-Z]{4}-[0-9a-zA-Z]{12}/.test(data.revisionId), true, "done has a valid revisionId");
        is (data.revisionId, gRevisions[gRevisions.length-1], "Last revision matches");
        is (data.id, null, "'done' operation wants a null id");
        break;

      case 'add':
      case 'update':
        if ('id' in step) {
          is(data.id, step.id, "next() add: id matches: " + data.id + " " + step.id);
        }

        if ('data' in step) {
          is(data.data, step.data, "next() add: data matches: " + data.data + " " + step.data);
        }

        break;

      case 'remove':
        if ('id' in step) {
          is(data.id, step.id, "next() add: id matches: " + data.id + " " + step.id);
        }

        break;
    }

    testCursor(cursor, steps);
  });
}

var tests = [
  // Test for GetDataStore
  testGetDataStores,

  // interface test
  testBasicInterface,

  // empty DataStore
  function() {
    var cursor = gStore.sync();
    var steps = [ { operation: 'clear' },
                  { operation: 'done' },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    gExpectedEvents = false;
    var cursor = gStore.sync('wrong revision ID');
    var steps = [ { operation: 'clear' },
                  { operation: 'done' },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[0]);
    var steps = [ { operation: 'done' },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  // Test add from scratch
  function() {
    gExpectedEvents = true;

    gStore.add(1).then(function(id) {
      gRevisions.push(gStore.revisionId);
      ok(true, "Item: " + id + " added");
    });
  },

  function() {
    gStore.add(2,"foobar").then(function(id) {
      gRevisions.push(gStore.revisionId);
      ok(true, "Item: " + id + " added");
    });
  },

  function() {
    gStore.add(3,3).then(function(id) {
      gRevisions.push(gStore.revisionId);
      ok(true, "Item: " + id + " added");
    });
  },

  function() {
    gExpectedEvents = false;
    var cursor = gStore.sync();
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 1 },
                  { operation: 'add', id: 3, data: 3 },
                  { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync('wrong revision ID');
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 1 },
                  { operation: 'add', id: 3, data: 3 },
                  { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[0]);
    var steps = [ { operation: 'add', id: 1, data: 1 },
                  { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'add', id: 3, data: 3 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[1]);
    var steps = [ { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'add', id: 3, data: 3 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[2]);
    var steps = [ { operation: 'add', id: 3, data: 3 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[3]);
    var steps = [ { operation: 'done' }];
    testCursor(cursor, steps);
  },

  // Test after an update
  function() {
    gExpectedEvents = true;
    gStore.put(123, 1).then(function() {
      gRevisions.push(gStore.revisionId);
    });
  },

  function() {
    gExpectedEvents = false;
    var cursor = gStore.sync();
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 3, data: 3 },
                  { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync('wrong revision ID');
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 3, data: 3 },
                  { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[0]);
    var steps = [ { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'add', id: 3, data: 3 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[1]);
    var steps = [ { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'add', id: 3, data: 3 },
                  { operation: 'update', id: 1, data: 123 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[2]);
    var steps = [ { operation: 'add', id: 3, data: 3 },
                  { operation: 'update', id: 1, data: 123 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[3]);
    var steps = [ { operation: 'update', id: 1, data: 123 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[4]);
    var steps = [ { operation: 'done' }];
    testCursor(cursor, steps);
  },

  // Test after a remove
  function() {
    gExpectedEvents = true;
    gStore.remove(3).then(function() {
      gRevisions.push(gStore.revisionId);
    });
  },

  function() {
    gExpectedEvents = false;
    var cursor = gStore.sync();
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync('wrong revision ID');
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[0]);
    var steps = [ { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[1]);
    var steps = [ { operation: 'add', id: 'foobar', data: 2 },
                  { operation: 'update', id: 1, data: 123 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[2]);
    var steps = [ { operation: 'update', id: 1, data: 123 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[3]);
    var steps = [ { operation: 'update', id: 1, data: 123 },
                  { operation: 'remove', id: 3 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[4]);
    var steps = [ { operation: 'remove', id: 3 },
                  { operation: 'done' }];
    testCursor(cursor, steps);
  },

  function() {
    var cursor = gStore.sync(gRevisions[5]);
    var steps = [ { operation: 'done' }];
    testCursor(cursor, steps);
  },

  // New events when the cursor is active
  function() {
    gCursor = gStore.sync();
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 'foobar', data: 2 } ];
    testCursor(gCursor, steps);
  },

  function() {
    gStore.add(42, 2).then(function(id) {
      ok(true, "Item: " + id + " added");
      gRevisions.push(gStore.revisionId);
      runTest();
    });
  },

  function() {
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 2, data: 42 },
                  { operation: 'add', id: 'foobar', data: 2 } ]
    testCursor(gCursor, steps);
  },

  function() {
    gStore.put(43, 2).then(function(id) {
      gRevisions.push(gStore.revisionId);
      runTest();
    });
  },

  function() {
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 2, data: 43 },
                  { operation: 'add', id: 'foobar', data: 2 } ]
    testCursor(gCursor, steps);
  },

  function() {
    gStore.remove(2).then(function(id) {
      gRevisions.push(gStore.revisionId);
      runTest();
    });
  },

  function() {
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 'foobar', data: 2 } ]
    testCursor(gCursor, steps);
  },

  function() {
    gStore.add(42).then(function(id) {
      ok(true, "Item: " + id + " added");
      gRevisions.push(gStore.revisionId);
      runTest();
    });
  },

  function() {
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 1, data: 123 },
                  { operation: 'add', id: 4, data: 42 },
                  { operation: 'add', id: 'foobar', data: 2 } ]
    testCursor(gCursor, steps);
  },

  function() {
    gStore.clear().then(function() {
      gRevisions.push(gStore.revisionId);
      runTest();
    });
  },

  function() {
    var steps = [ { operation: 'clear' } ];
    testCursor(gCursor, steps);
  },

  function() {
    gStore.add(42).then(function(id) {
      ok(true, "Item: " + id + " added");
      gRevisions.push(gStore.revisionId);
      runTest();
    });
  },

  function() {
    var steps = [ { operation: 'clear', },
                  { operation: 'add', id: 5, data: 42 } ];
    testCursor(gCursor, steps);
  },

  function() {
    gStore.clear().then(function() {
      gRevisions.push(gStore.revisionId);
      runTest();
    });
  },

  function() {
    gStore.add(42).then(function(id) {
      ok(true, "Item: " + id + " added");
      gRevisions.push(gStore.revisionId);
      runTest();
    });
  },

  function() {
    var steps = [ { operation: 'clear' },
                  { operation: 'add', id: 6, data: 42 },
                  { operation: 'done'} ];
    testCursor(gCursor, steps);
  },

  function() {
    gExpectedEvents = true;
    gStore.add(42).then(function(id) {
    });
  }
];

function runTest() {
  if (!tests.length) {
    finish();
    return;
  }

  var test = tests.shift();
  test();
}
