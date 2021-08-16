// ================================================================================================
// ================================================================================================
add_task(async function runRPTests() {
  let runTests = async function(data) {
    let timerlist = data.list;
    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    ok(
      isRounded(content.performance.timeOrigin, expectedPrecision),
      `For resistFingerprinting, performance.timeOrigin is not correctly rounded: ` +
        content.performance.timeOrigin
    );

    // Check that the performance timing API is correctly spoofed. In
    // particular, check if domainLookupStart and domainLookupEnd return
    // fetchStart, and if everything else is clamped to the expected precision.
    for (let time of timerlist) {
      if (time == "domainLookupStart" || time == "domainLookupEnd") {
        is(
          content.performance.timing[time],
          content.performance.timing.fetchStart,
          `For resistFingerprinting, the timing(${time}) is not correctly spoofed.`
        );
      } else {
        ok(
          isRounded(content.performance.timing[time], expectedPrecision),
          `For resistFingerprinting with expected precision ` +
            expectedPrecision +
            `, the timing(${time}) is not correctly rounded: ` +
            content.performance.timing[time]
        );
      }
    }

    // Try to add some entries.
    content.performance.mark("Test");
    content.performance.mark("Test-End");
    content.performance.measure("Test-Measure", "Test", "Test-End");

    // Check that no entries for performance.getEntries/getEntriesByType/getEntriesByName.
    is(
      content.performance.getEntries().length,
      0,
      "For resistFingerprinting, there should be no entries for performance.getEntries()"
    );
    is(
      content.performance.getEntriesByType("resource").length,
      0,
      "For resistFingerprinting, there should be no entries for performance.getEntriesByType()"
    );
    is(
      content.performance.getEntriesByName("Test", "mark").length,
      0,
      "For resistFingerprinting, there should be no entries for performance.getEntriesByName()"
    );
  };

  await setupPerformanceAPISpoofAndDisableTest(
    true,
    true,
    false,
    200,
    runTests
  );
  await setupPerformanceAPISpoofAndDisableTest(
    true,
    true,
    false,
    100,
    runTests
  );
  await setupPerformanceAPISpoofAndDisableTest(
    true,
    false,
    false,
    13,
    runTests
  );
  await setupPerformanceAPISpoofAndDisableTest(
    true,
    false,
    false,
    0.13,
    runTests
  );
  await setupPerformanceAPISpoofAndDisableTest(true, true, true, 100, runTests);
  await setupPerformanceAPISpoofAndDisableTest(true, false, true, 13, runTests);
  await setupPerformanceAPISpoofAndDisableTest(
    true,
    false,
    true,
    0.13,
    runTests
  );
});

// ================================================================================================
// ================================================================================================
add_task(async function runRTPTests() {
  let runTests = async function(data) {
    let timerlist = data.list;
    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    ok(
      isRounded(content.performance.timeOrigin, expectedPrecision),
      `For reduceTimerPrecision, performance.timeOrigin is not correctly rounded: ` +
        content.performance.timeOrigin
    );

    // Check that whether the performance timing API is correctly spoofed.
    for (let time of timerlist) {
      ok(
        isRounded(content.performance.timing[time], expectedPrecision),
        `For reduceTimerPrecision(` +
          expectedPrecision +
          `), the timing(${time}) is not correctly rounded: ` +
          content.performance.timing[time]
      );
    }

    // Try to add some entries.
    content.performance.mark("Test");
    content.performance.mark("Test-End");
    content.performance.measure("Test-Measure", "Test", "Test-End");

    // Check the entries for performance.getEntries/getEntriesByType/getEntriesByName.
    await new Promise(resolve => {
      const paintObserver = new content.PerformanceObserver(() => {
        resolve();
      });
      paintObserver.observe({ type: "paint", buffered: true });
    });

    is(
      content.performance.getEntries().length,
      5,
      "For reduceTimerPrecision, there should be 4 entries for performance.getEntries()"
      // PerformancePaintTiming, PerformanceNavigationTiming, PerformanceMark, PerformanceMark, PerformanceMeasure
    );
    for (var i = 0; i < 5; i++) {
      let startTime = content.performance.getEntries()[i].startTime;
      let duration = content.performance.getEntries()[i].duration;
      ok(
        isRounded(startTime, expectedPrecision),
        "For reduceTimerPrecision(" +
          expectedPrecision +
          "), performance.getEntries(" +
          i +
          ").startTime is not rounded: " +
          startTime
      );
      ok(
        isRounded(duration, expectedPrecision),
        "For reduceTimerPrecision(" +
          expectedPrecision +
          "), performance.getEntries(" +
          i +
          ").duration is not rounded: " +
          duration
      );
    }
    is(
      content.performance.getEntriesByType("mark").length,
      2,
      "For reduceTimerPrecision, there should be 2 entries for performance.getEntriesByType()"
    );
    is(
      content.performance.getEntriesByName("Test", "mark").length,
      1,
      "For reduceTimerPrecision, there should be 1 entry for performance.getEntriesByName()"
    );
    content.performance.clearMarks();
    content.performance.clearMeasures();
    content.performance.clearResourceTimings();
  };

  await setupPerformanceAPISpoofAndDisableTest(
    false,
    true,
    false,
    100,
    runTests
  );
  await setupPerformanceAPISpoofAndDisableTest(
    false,
    true,
    false,
    13,
    runTests
  );
  await setupPerformanceAPISpoofAndDisableTest(
    false,
    true,
    false,
    0.13,
    runTests
  );
  await setupPerformanceAPISpoofAndDisableTest(
    false,
    true,
    true,
    0.005,
    runTests
  );
});
