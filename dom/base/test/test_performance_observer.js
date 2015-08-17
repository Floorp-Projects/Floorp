setup({ explicit_done: true });

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

  assert_throws({name: "TypeError"}, function() {
    observer.observe();
  }, "observe() should throw TypeError exception if no option specified.");

  assert_throws({name: "TypeError"}, function() {
    observer.observe({ unsupportedAttribute: "unsupported" });
  }, "obsrve() should throw TypeError exception if the option has no 'entryTypes' attribute.");

  assert_throws({name: "TypeError"}, function() {
    observer.observe({ entryTypes: [] });
  }, "obsrve() should throw TypeError exception if 'entryTypes' attribute is an empty sequence.");

  assert_throws({name: "TypeError"}, function() {
    observer.observe({ entryTypes: null });
  }, "obsrve() should throw TypeError exception if 'entryTypes' attribute is null.");

  assert_throws({name: "TypeError"}, function() {
    observer.observe({ entryTypes: ["invalid"]});
  }, "obsrve() should throw TypeError exception if 'entryTypes' attribute value is invalid.");
}, "Test that PerformanceObserver.observe throws exception");

test(t => {
  performance.clearMarks();
  performance.clearMeasures();

  var observedEntries = [];
  var observer = new PerformanceObserver(list => {
    list.getEntries().forEach(entry => observedEntries.push(entry));
  });
  observer.observe({entryTypes: ['mark', 'measure']});

  assert_equals(observedEntries.length, 0,
                "User timing entries should never be observed.");
  assert_equals(performance.getEntriesByType("mark").length, 0,
                "There should be no 'mark' entries.");
  assert_equals(performance.getEntriesByType("measure").length, 0,
                "There should be no 'measure' entries.");

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  assert_equals(observedEntries.length, 3,
                "There should be three observed entries.");

  var markEntries = observedEntries.filter(entry => {
    return entry.entryType == "mark";
  });
  assert_array_equals(markEntries, performance.getEntriesByType("mark"),
                      "Observed 'mark' entries should equal to entries obtained by getEntriesByType.");

  var measureEntries = observedEntries.filter(entry => {
    return entry.entryType == "measure";
  });
  assert_array_equals(measureEntries, performance.getEntriesByType("measure"),
                      "Observed 'measure' entries should equal to entries obtained by getEntriesByType.");
}, "Test for user-timing with PerformanceObserver");

test(t => {
  performance.clearMarks();
  performance.clearMeasures();

  var observedEntries = [];
  var observer = new PerformanceObserver(list => {
    list.getEntries().forEach(entry => observedEntries.push(entry));
  });
  observer.observe({entryTypes: ['mark', 'measure']});

  observer.disconnect();

  assert_equals(observedEntries.length, 0,
                "User timing entries should be never observed.");

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  assert_equals(performance.getEntriesByType("mark").length, 2);
  assert_equals(performance.getEntriesByType("measure").length, 1);

  assert_equals(observedEntries.length, 0,
                "User timing entries should be never observed after disconnecting observer.");
}, "Nothing should be notified after disconnecting observer");

test(t => {
  performance.clearMarks();
  performance.clearMeasures();

  var observedEntryList;
  var observer = new PerformanceObserver(list => {
    observedEntryList = list;
  });
  observer.observe({entryTypes: ['mark']});

  performance.mark("test");
  assert_array_equals(observedEntryList.getEntries({"entryType": "mark"}),
                      performance.getEntriesByType("mark"),
                      "getEntries with entryType filter should return correct results.");

  assert_array_equals(observedEntryList.getEntries({"name": "test"}),
                      performance.getEntriesByName("test"),
                      "getEntries with name filter should return correct results.");

  assert_array_equals(observedEntryList.getEntries({"name": "test",
                                                    "entryType": "mark"}),
                      performance.getEntriesByName("test"),
                      "getEntries with name and entryType filter should return correct results.");

  assert_array_equals(observedEntryList.getEntries({"name": "invalid"}),
                      [],
                      "getEntries with non-existent name filter should return an empty array.");

  assert_array_equals(observedEntryList.getEntries({"name": "test",
                                                    "entryType": "measure"}),
                      [],
                      "getEntries with name filter and non-existent entryType should return an empty array.");

  assert_array_equals(observedEntryList.getEntries({"name": "invalid",
                                                    "entryType": "mark"}),
                      [],
                      "getEntries with non-existent name and entryType filter should return an empty array.");
}, "Test for PerformanceObserverEntryList.getEntries");

test(t => {
  performance.clearMarks();
  performance.clearMeasures();

  var observedEntryList;
  var observer = new PerformanceObserver(list => {
    observedEntryList = list;
  });
  observer.observe({entryTypes: ['mark', 'measure']});

  performance.mark("test");
  assert_array_equals(observedEntryList.getEntriesByType("mark"),
                      performance.getEntriesByType("mark"));

  performance.measure("test-measure", "test", "test");
  assert_array_equals(observedEntryList.getEntriesByType("measure"),
                      performance.getEntriesByType("measure"));
}, "Test for PerformanceObserverEntryList.getEntriesByType");

test(t => {
  performance.clearMarks();
  performance.clearMeasures();

  var observedEntryList;
  var observer = new PerformanceObserver(list => {
    observedEntryList = list;
  });
  observer.observe({entryTypes: ['mark', 'measure']});

  performance.mark("test");
  assert_array_equals(observedEntryList.getEntriesByName("test"),
                      performance.getEntriesByName("test"));

  performance.measure("test-measure", "test", "test");
  assert_array_equals(observedEntryList.getEntriesByName("test-measure"),
                      performance.getEntriesByName("test-measure"));
}, "Test for PerformanceObserverEntryList.getEntriesByName");

test(t => {
  performance.clearMarks();
  performance.clearMeasures();

  var observedEntries = [];
  var observer = new PerformanceObserver(list => {
    list.getEntries().forEach(entry => observedEntries.push(entry));
  });

  observer.observe({entryTypes: ['mark', 'measure']});
  observer.observe({entryTypes: ['mark', 'measure']});

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  assert_equals(observedEntries.length, 3,
                "Observed user timing entries should have only three entries.");
}, "Test that invoking observe method twice affects nothing");

test(t => {
  performance.clearMarks();
  performance.clearMeasures();

  var observedEntries = [];
  var observer = new PerformanceObserver(list => {
    list.getEntries().forEach(entry => observedEntries.push(entry));
  });

  observer.observe({entryTypes: ['mark', 'measure']});
  observer.observe({entryTypes: ['mark']});

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  assert_equals(observedEntries.length, 2,
                "Observed user timing entries should have only two entries.");
}, "Test that observing filter is replaced by a new filter");

test(t => {
  performance.clearMarks();
  performance.clearMeasures();

  var observedEntries = [];
  var observer = new PerformanceObserver(list => {
    list.getEntries().forEach(entry => observedEntries.push(entry));
  });

  observer.observe({entryTypes: ['mark']});
  observer.observe({entryTypes: ['measure']});

  performance.mark("test-start");
  performance.mark("test-end");
  performance.measure("test-measure", "test-start", "test-end");

  assert_equals(observedEntries.length, 1,
                "Observed user timing entries should have only 1 entries.");
}, "Test that observing filter is replaced by a new filter");
