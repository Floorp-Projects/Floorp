var steps = [
  // Test single mark addition
  function() {
    ok(true, "Running mark addition test");
    performance.mark("test");
    var marks = performance.getEntriesByType("mark");
    is(marks.length, 1, "Number of marks should be 1");
    var mark = marks[0];
    is(mark.name, "test", "mark name should be 'test'");
    is(mark.entryType, "mark", "mark type should be 'mark'");
    isnot(mark.startTime, 0, "mark start time should not be 0");
    is(mark.duration, 0, "mark duration should be 0");
  },
  // Test multiple mark addition
  function() {
    ok(true, "Running multiple mark with same name addition test");
    performance.mark("test");
    performance.mark("test");
    performance.mark("test");
    var marks_type = performance.getEntriesByType("mark");
    is(marks_type.length, 3, "Number of marks by type should be 3");
    var marks_name = performance.getEntriesByName("test");
    is(marks_name.length, 3, "Number of marks by name should be 3");
    var mark = marks_name[0];
    is(mark.name, "test", "mark name should be 'test'");
    is(mark.entryType, "mark", "mark type should be 'mark'");
    isnot(mark.startTime, 0, "mark start time should not be 0");
    is(mark.duration, 0, "mark duration should be 0");
    var times = [];
    // This also tests the chronological ordering specified as
    // required for getEntries in the performance timeline spec.
    marks_name.forEach(function(s) {
      times.forEach(function(time) {
        ok(
          s.startTime >= time.startTime,
          "Times should be equal or increasing between similarly named marks: " +
            s.startTime +
            " >= " +
            time.startTime
        );
      });
      times.push(s);
    });
  },
  // Test all marks removal
  function() {
    ok(true, "Running all mark removal test");
    performance.mark("test");
    performance.mark("test2");
    var marks = performance.getEntriesByType("mark");
    is(marks.length, 2, "number of marks before all removal");
    performance.clearMarks();
    marks = performance.getEntriesByType("mark");
    is(marks.length, 0, "number of marks after all removal");
  },
  // Test single mark removal
  function() {
    ok(true, "Running removal test (0 'test' marks with other marks)");
    performance.mark("test2");
    var marks = performance.getEntriesByType("mark");
    is(marks.length, 1, "number of marks before all removal");
    performance.clearMarks("test");
    marks = performance.getEntriesByType("mark");
    is(marks.length, 1, "number of marks after all removal");
  },
  // Test single mark removal
  function() {
    ok(true, "Running removal test (0 'test' marks with no other marks)");
    var marks = performance.getEntriesByType("mark");
    is(marks.length, 0, "number of marks before all removal");
    performance.clearMarks("test");
    marks = performance.getEntriesByType("mark");
    is(marks.length, 0, "number of marks after all removal");
  },
  function() {
    ok(true, "Running removal test (1 'test' mark with other marks)");
    performance.mark("test");
    performance.mark("test2");
    var marks = performance.getEntriesByType("mark");
    is(marks.length, 2, "number of marks before all removal");
    performance.clearMarks("test");
    marks = performance.getEntriesByType("mark");
    is(marks.length, 1, "number of marks after all removal");
  },
  function() {
    ok(true, "Running removal test (1 'test' mark with no other marks)");
    performance.mark("test");
    var marks = performance.getEntriesByType("mark");
    is(marks.length, 1, "number of marks before all removal");
    performance.clearMarks("test");
    marks = performance.getEntriesByType("mark");
    is(marks.length, 0, "number of marks after all removal");
  },
  function() {
    ok(true, "Running removal test (2 'test' marks with other marks)");
    performance.mark("test");
    performance.mark("test");
    performance.mark("test2");
    var marks = performance.getEntriesByType("mark");
    is(marks.length, 3, "number of marks before all removal");
    performance.clearMarks("test");
    marks = performance.getEntriesByType("mark");
    is(marks.length, 1, "number of marks after all removal");
  },
  function() {
    ok(true, "Running removal test (2 'test' marks with no other marks)");
    performance.mark("test");
    performance.mark("test");
    var marks = performance.getEntriesByType("mark");
    is(marks.length, 2, "number of marks before all removal");
    performance.clearMarks("test");
    marks = performance.getEntriesByType("mark");
    is(marks.length, 0, "number of marks after all removal");
  },
  // Test mark name being same as navigation timing parameter
  function() {
    ok(true, "Running mark name collision test");
    for (n in performance.timing) {
      try {
        if (n == "toJSON") {
          ok(true, "Skipping toJSON entry in collision test");
          continue;
        }
        performance.mark(n);
        ok(
          false,
          "Mark name collision test failed for name " +
            n +
            ", shouldn't make it here!"
        );
      } catch (e) {
        ok(
          e instanceof DOMException,
          "DOM exception thrown for mark named " + n
        );
        is(
          e.code,
          e.SYNTAX_ERR,
          "DOM exception for name collision is syntax error"
        );
      }
    }
  },
  // Test measure
  function() {
    ok(true, "Running measure addition with no start/end time test");
    performance.measure("test");
    var measures = performance.getEntriesByType("measure");
    is(measures.length, 1, "number of measures should be 1");
    var measure = measures[0];
    is(measure.name, "test", "measure name should be 'test'");
    is(measure.entryType, "measure", "measure type should be 'measure'");
    is(measure.startTime, 0, "measure start time should be zero");
    ok(measure.duration >= 0, "measure duration should not be negative");
  },
  function() {
    ok(true, "Running measure addition with only start time test");
    performance.mark("test1");
    performance.measure("test", "test1", undefined);
    var measures = performance.getEntriesByName("test", "measure");
    var marks = performance.getEntriesByName("test1", "mark");
    var measure = measures[0];
    var mark = marks[0];
    is(
      measure.startTime,
      mark.startTime,
      "measure start time should be equal to the mark startTime"
    );
    ok(measure.duration >= 0, "measure duration should not be negative");
  },
  function() {
    ok(true, "Running measure addition with only end time test");
    performance.mark("test1");
    performance.measure("test", undefined, "test1");
    var measures = performance.getEntriesByName("test", "measure");
    var marks = performance.getEntriesByName("test1", "mark");
    var measure = measures[0];
    var mark = marks[0];
    ok(measure.duration >= 0, "measure duration should not be negative");
  },
  // Test measure picking latest version of similarly named tags
  function() {
    ok(true, "Running multiple mark with same name addition test");
    performance.mark("test");
    performance.mark("test");
    performance.mark("test");
    performance.mark("test2");
    var marks_name = performance.getEntriesByName("test");
    is(marks_name.length, 3, "Number of marks by name should be 3");
    var marks_name2 = performance.getEntriesByName("test2");
    is(marks_name2.length, 1, "Number of marks by name should be 1");
    var test_mark = marks_name2[0];
    performance.measure("test", "test", "test2");
    var measures_type = performance.getEntriesByType("measure");
    var last_mark = marks_name[marks_name.length - 1];
    is(measures_type.length, 1, "Number of measures by type should be 1");
    var measure = measures_type[0];
    is(
      measure.startTime,
      last_mark.startTime,
      "Measure start time should be the start time of the latest 'test' mark"
    );
    // Tolerance testing to avoid oranges, since we're doing double math across two different languages.
    ok(
      measure.duration - (test_mark.startTime - last_mark.startTime) < 0.00001,
      "Measure duration ( " +
        measure.duration +
        ") should be difference between two marks"
    );
  },
  function() {
    // We don't have navigationStart in workers.
    if ("window" in self) {
      ok(true, "Running measure addition with no start/end time test");
      performance.measure("test", "navigationStart");
      var measures = performance.getEntriesByType("measure");
      is(measures.length, 1, "number of measures should be 1");
      var measure = measures[0];
      is(measure.name, "test", "measure name should be 'test'");
      is(measure.entryType, "measure", "measure type should be 'measure'");
      is(measure.startTime, 0, "measure start time should be zero");
      ok(measure.duration >= 0, "measure duration should not be negative");
    }
  },
  // Test all measure removal
  function() {
    ok(true, "Running all measure removal test");
    performance.measure("test");
    performance.measure("test2");
    var measures = performance.getEntriesByType("measure");
    is(measures.length, 2, "measure entries should be length 2");
    performance.clearMeasures();
    measures = performance.getEntriesByType("measure");
    is(measures.length, 0, "measure entries should be length 0");
  },
  // Test single measure removal
  function() {
    ok(true, "Running all measure removal test");
    performance.measure("test");
    performance.measure("test2");
    var measures = performance.getEntriesByType("measure");
    is(measures.length, 2, "measure entries should be length 2");
    performance.clearMeasures("test");
    measures = performance.getEntriesByType("measure");
    is(measures.length, 1, "measure entries should be length 1");
  },
  // Test measure with invalid start time mark name
  function() {
    ok(true, "Running measure invalid start test");
    try {
      performance.measure("test", "notamark");
      ok(false, "invalid measure start time exception not thrown!");
    } catch (e) {
      ok(e instanceof DOMException, "DOM exception thrown for invalid measure");
      is(
        e.code,
        e.SYNTAX_ERR,
        "DOM exception for invalid time is syntax error"
      );
    }
  },
  // Test measure with invalid end time mark name
  function() {
    ok(true, "Running measure invalid end test");
    try {
      performance.measure("test", undefined, "notamark");
      ok(false, "invalid measure end time exception not thrown!");
    } catch (e) {
      ok(e instanceof DOMException, "DOM exception thrown for invalid measure");
      is(
        e.code,
        e.SYNTAX_ERR,
        "DOM exception for invalid time is syntax error"
      );
    }
  },
  // Test measure name being same as navigation timing parameter
  function() {
    ok(true, "Running measure name collision test");
    for (n in performance.timing) {
      if (n == "toJSON") {
        ok(true, "Skipping toJSON entry in collision test");
        continue;
      }
      performance.measure(n);
      ok(true, "Measure name supports name collisions: " + n);
    }
  },
  // Test measure mark being a reserved name
  function() {
    ok(true, "Create measures using all reserved names");
    for (n in performance.timing) {
      try {
        if (n == "toJSON") {
          ok(true, "Skipping toJSON entry in collision test");
          continue;
        }
        performance.measure("test", n);
        ok(true, "Measure created from reserved name as starting time: " + n);
      } catch (e) {
        ok(
          [
            "redirectStart",
            "redirectEnd",
            "unloadEventStart",
            "unloadEventEnd",
            "loadEventEnd",
            "secureConnectionStart",
          ].includes(n),
          "Measure created from reserved name as starting time: " +
            n +
            " and threw expected error"
        );
      }
    }
  },
  // TODO: Test measure picking latest version of similarly named tags
];
