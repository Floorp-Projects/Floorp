function test_registerFailure() {
  ok("sync" in navigator, "navigator.sync exists");

  navigator.sync.register().then(
  function() {
    ok(false, "navigator.sync.register() throws without a task name");
  }, function() {
    ok(true, "navigator.sync.register() throws without a task name");
  })

  .then(function() {
    return navigator.sync.register(42);
  }).then(function() {
    ok(false, "navigator.sync.register() throws without a string task name");
  }, function() {
    ok(true, "navigator.sync.register() throws without a string task name");
  })

  .then(function() {
    return navigator.sync.register('foobar');
  }).then(function() {
    ok(false, "navigator.sync.register() throws without a param dictionary");
  }, function() {
    ok(true, "navigator.sync.register() throws without a param dictionary");
  })

  .then(function() {
    return navigator.sync.register('foobar', 42);
  }).then(function() {
    ok(false, "navigator.sync.register() throws without a real dictionary");
  }, function() {
    ok(true, "navigator.sync.register() throws without a real dictionary");
  })

  .then(function() {
    return navigator.sync.register('foobar', {});
  }).then(function() {
    ok(false, "navigator.sync.register() throws without a minInterval and wakeUpPage");
  }, function() {
    ok(true, "navigator.sync.register() throws without a minInterval and wakeUpPage");
  })

  .then(function() {
    return navigator.sync.register('foobar', { minInterval: 100 });
  }).then(function() {
    ok(false, "navigator.sync.register() throws without a wakeUpPage");
  }, function() {
    ok(true, "navigator.sync.register() throws without a wakeUpPage");
  })

  .then(function() {
    return navigator.sync.register('foobar', { wakeUpPage: 100 });
  }).then(function() {
    ok(false, "navigator.sync.register() throws without a minInterval");
  }, function() {
    ok(true, "navigator.sync.register() throws without a minInterval");
  })

  .then(function() {
    runTests();
  });
}

function genericError() {
  ok(false, "Some promise failed");
}

function test_register() {
  navigator.sync.register('foobar', { minInterval: 5, wakeUpPage:'/' }).then(
  function() {
    ok(true, "navigator.sync.register() worked!");
    runTests();
  }, genericError);
}

function test_unregister() {
  navigator.sync.unregister('foobar').then(
  function() {
    ok(true, "navigator.sync.unregister() worked!");
    runTests();
  }, genericError);
}

function test_unregisterDuplicate() {
  navigator.sync.unregister('foobar').then(
  genericError,
  function(error) {
    ok(true, "navigator.sync.unregister() should throw if the task doesn't exist.");
    ok(error, "UnknownTaskError", "Duplicate unregistration error is correct");
    runTests();
  });
}

function test_registrationEmpty() {
  navigator.sync.registration('bar').then(
  function(results) {
    is(results, null, "navigator.sync.registration() should return null.");
    runTests();
  },
  genericError);
}

function test_registration() {
  navigator.sync.registration('foobar').then(
  function(results) {
    is(results.task, 'foobar', "navigator.sync.registration().task is correct");
    ok("lastSync" in results, "navigator.sync.registration().lastSync is correct");
    is(results.oneShot, true, "navigator.sync.registration().oneShot is correct");
    is(results.minInterval, 5, "navigator.sync.registration().minInterval is correct");
    ok("wakeUpPage" in results, "navigator.sync.registration().wakeUpPage is correct");
    ok("wifiOnly" in results, "navigator.sync.registration().wifiOnly is correct");
    ok("data" in results, "navigator.sync.registration().data is correct");
    ok(!("app" in results), "navigator.sync.registrations().app is correct");
    runTests();
  },
  genericError);
}

function test_registrationsEmpty() {
  navigator.sync.registrations().then(
  function(results) {
    is(results.length, 0, "navigator.sync.registrations() should return an empty array.");
    runTests();
  },
  genericError);
}

function test_registrations() {
  navigator.sync.registrations().then(
  function(results) {
    is(results.length, 1, "navigator.sync.registrations() should not return an empty array.");
    is(results[0].task, 'foobar', "navigator.sync.registrations()[0].task is correct");
    ok("lastSync" in results[0], "navigator.sync.registrations()[0].lastSync is correct");
    is(results[0].oneShot, true, "navigator.sync.registrations()[0].oneShot is correct");
    is(results[0].minInterval, 5, "navigator.sync.registrations()[0].minInterval is correct");
    ok("wakeUpPage" in results[0], "navigator.sync.registration()[0].wakeUpPage is correct");
    ok("wifiOnly" in results[0], "navigator.sync.registrations()[0].wifiOnly is correct");
    ok("data" in results[0], "navigator.sync.registrations()[0].data is correct");
    ok(!("app" in results[0]), "navigator.sync.registrations()[0].app is correct");
    runTests();
  },
  genericError);
}

function test_managerRegistrationsEmpty() {
  navigator.syncManager.registrations().then(
  function(results) {
    is(results.length, 0, "navigator.syncManager.registrations() should return an empty array.");
    runTests();
  },
  genericError);
}

function test_managerRegistrations(state, overwrittenMinInterval) {
  navigator.syncManager.registrations().then(
  function(results) {
    is(results.length, 1, "navigator.sync.registrations() should not return an empty array.");
    is(results[0].task, 'foobar', "navigator.sync.registrations()[0].task is correct");
    ok("lastSync" in results[0], "navigator.sync.registrations()[0].lastSync is correct");
    is(results[0].oneShot, true, "navigator.sync.registrations()[0].oneShot is correct");
    is(results[0].minInterval, 5, "navigator.sync.registrations()[0].minInterval is correct");
    ok("wakeUpPage" in results[0], "navigator.sync.registration()[0].wakeUpPage is correct");
    ok("wifiOnly" in results[0], "navigator.sync.registrations()[0].wifiOnly is correct");
    ok("data" in results[0], "navigator.sync.registrations()[0].data is correct");
    ok("app" in results[0], "navigator.sync.registrations()[0].app is correct");
    ok("manifestURL" in results[0].app, "navigator.sync.registrations()[0].app.manifestURL is correct");
    is(results[0].app.origin, 'http://mochi.test:8888', "navigator.sync.registrations()[0].app.origin is correct");
    is(results[0].app.isInBrowserElement, false, "navigator.sync.registrations()[0].app.isInBrowserElement is correct");
    is(results[0].state, state, "navigator.sync.registrations()[0].state is correct");
    is(results[0].overwrittenMinInterval, overwrittenMinInterval, "navigator.sync.registrations()[0].overwrittenMinInterval is correct");
    ok("setPolicy" in results[0], "navigator.sync.registrations()[0].setPolicy is correct");
    runTests();
  },
  genericError);
}

function test_managerSetPolicy(state, overwrittenMinInterval) {
  navigator.syncManager.registrations().then(
  function(results) {
    results[0].setPolicy(state, overwrittenMinInterval).then(
    function() {
      ok(state, results[0].state, "State matches");
      ok(overwrittenMinInterval, results[0].overwrittenMinInterval, "OverwrittenMinInterval matches");
      runTests();
    }, genericError);
  }).catch(genericError);
}
