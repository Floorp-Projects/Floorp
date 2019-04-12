test(t => {
  assert_throws({name: "TypeError"}, function() {
    new PerformanceObserver();
  }, "PerformanceObserver constructor should throw TypeError if no argument is specified.");

  assert_throws({name: "TypeError"}, function() {
    new PerformanceObserver({});
  }, "PerformanceObserver constructor should throw TypeError if the argument is not a function.");
}, "Test that PerformanceObserver constructor throws exception");

test(t => {
  var observer = new PerformanceObserver(() => {
  });

  assert_throws({name: "SyntaxError"}, function() {
    observer.observe();
  }, "observe() should throw TypeError exception if no option specified.");

  assert_throws({name: "SyntaxError"}, function() {
    observer.observe({ unsupportedAttribute: "unsupported" });
  }, "obsrve() should throw TypeError exception if the option has no 'entryTypes' attribute.");

  assert_equals(undefined, observer.observe({ entryTypes: [] }),
     "observe() should silently ignore empty 'entryTypes' sequence.");

  assert_throws({name: "TypeError"}, function() {
    observer.observe({ entryTypes: null });
  }, "obsrve() should throw TypeError exception if 'entryTypes' attribute is null.");

  assert_equals(undefined, observer.observe({ entryTypes: ["invalid"] }),
     "observe() should silently ignore invalid 'entryTypes' values.");
}, "Test that PerformanceObserver.observe throws exception");

function promiseObserve(test, options) {
  return new Promise(resolve => {
    performance.clearMarks();
    performance.clearMeasures();

    var observer = new PerformanceObserver(list => resolve(list));
    observer.observe(options);
    test.add_cleanup(() => observer.disconnect());
  });
}

promise_test(t => {
  var promise = promiseObserve(t, {entryTypes: ['mark', 'measure']});

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  return promise.then(list => {
    assert_equals(list.getEntries().length, 3,
      "There should be three observed entries.");

    var markEntries = list.getEntries().filter(entry => {
      return entry.entryType == "mark";
    });
    assert_array_equals(markEntries, performance.getEntriesByType("mark"),
      "Observed 'mark' entries should equal to entries obtained by getEntriesByType.");

    var measureEntries = list.getEntries().filter(entry => {
      return entry.entryType == "measure";
    });
    assert_array_equals(measureEntries, performance.getEntriesByType("measure"),
      "Observed 'measure' entries should equal to entries obtained by getEntriesByType.");
  });
}, "Test for user-timing with PerformanceObserver");

promise_test(t => {
  var promise = new Promise((resolve, reject) => {
    performance.clearMarks();
    performance.clearMeasures();

    var observer = new PerformanceObserver(list => reject(list));
    observer.observe({entryTypes: ['mark', 'measure']});
    observer.disconnect();
    t.step_timeout(resolve, 100);
  });

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  return promise.then(() => {
    assert_equals(performance.getEntriesByType("mark").length, 2);
    assert_equals(performance.getEntriesByType("measure").length, 1);
  }, list => {
    assert_unreached("Observer callback should never be called.");
  });

}, "Nothing should be notified after disconnecting observer");

promise_test(t => {
  var promise = promiseObserve(t, {entryTypes: ['mark']});

  performance.mark("test");

  return promise.then(list => {
    assert_array_equals(list.getEntries({"entryType": "mark"}),
                        performance.getEntriesByType("mark"),
                        "getEntries with entryType filter should return correct results.");

    assert_array_equals(list.getEntries({"name": "test"}),
                        performance.getEntriesByName("test"),
                        "getEntries with name filter should return correct results.");

    assert_array_equals(list.getEntries({"name": "test",
                                                      "entryType": "mark"}),
                        performance.getEntriesByName("test"),
                        "getEntries with name and entryType filter should return correct results.");

    assert_array_equals(list.getEntries({"name": "invalid"}),
                        [],
                        "getEntries with non-existent name filter should return an empty array.");

    assert_array_equals(list.getEntries({"name": "test",
                                                      "entryType": "measure"}),
                        [],
                        "getEntries with name filter and non-existent entryType should return an empty array.");

    assert_array_equals(list.getEntries({"name": "invalid",
                                                      "entryType": "mark"}),
                        [],
                        "getEntries with non-existent name and entryType filter should return an empty array.");

    assert_array_equals(list.getEntries({initiatorType: "xmlhttprequest"}),
                        [],
                        "getEntries with initiatorType filter should return an empty array.");
  });
}, "Test for PerformanceObserverEntryList.getEntries");

promise_test(t => {
  var promise = promiseObserve(t, {entryTypes: ['mark', 'measure']});

  performance.mark("test");
  performance.measure("test-measure", "test", "test");

  return promise.then(list => {
    assert_array_equals(list.getEntriesByType("mark"),
                        performance.getEntriesByType("mark"));
    assert_array_equals(list.getEntriesByType("measure"),
                        performance.getEntriesByType("measure"));
  });
}, "Test for PerformanceObserverEntryList.getEntriesByType");

promise_test(t => {
  var promise = promiseObserve(t, {entryTypes: ['mark', 'measure']});

  performance.mark("test");
  performance.measure("test-measure", "test", "test");

  return promise.then(list => {
    assert_array_equals(list.getEntriesByName("test"),
                        performance.getEntriesByName("test"));
    assert_array_equals(list.getEntriesByName("test-measure"),
                        performance.getEntriesByName("test-measure"));
  });
}, "Test for PerformanceObserverEntryList.getEntriesByName");

promise_test(t => {
  var promise = new Promise(resolve => {
    performance.clearMarks();
    performance.clearMeasures();

    var observer = new PerformanceObserver(list => resolve(list));
    observer.observe({entryTypes: ['mark', 'measure']});
    observer.observe({entryTypes: ['mark', 'measure']});
    t.add_cleanup(() => observer.disconnect());
  });

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  return promise.then(list => {
    assert_equals(list.getEntries().length, 3,
                  "Observed user timing entries should have only three entries.");
  });
}, "Test that invoking observe method twice affects nothing");

promise_test(t => {
  var promise = new Promise(resolve => {
    performance.clearMarks();
    performance.clearMeasures();

    var observer = new PerformanceObserver(list => resolve(list));
    observer.observe({entryTypes: ['mark', 'measure']});
    observer.observe({entryTypes: ['mark']});
    t.add_cleanup(() => observer.disconnect());
  });

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  return promise.then(list => {
    assert_equals(list.getEntries().length, 2,
                  "Observed user timing entries should have only two entries.");
  });
}, "Test that observing filter is replaced by a new filter");

promise_test(t => {
  var promise = new Promise(resolve => {
    performance.clearMarks();
    performance.clearMeasures();

    var observer = new PerformanceObserver(list => resolve(list));
    observer.observe({entryTypes: ['mark']});
    observer.observe({entryTypes: ['measure']});
    t.add_cleanup(() => observer.disconnect());
  });

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  return promise.then(list => {
    assert_equals(list.getEntries().length, 1,
                  "Observed user timing entries should have only 1 entries.");
  });
}, "Test that observing filter is replaced by a new filter");
