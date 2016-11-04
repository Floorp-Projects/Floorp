"use strict";

/**
 * Tests the FX_TAB_SWITCH_SPINNER_VISIBLE_MS and
 * FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS telemetry probes
 */
let gMinHangTime = 500; // ms
let gMaxHangTime = 5 * 1000; // ms

/**
 * Make a data URI for a generic webpage with a script that hangs for a given
 * amount of time.
 * @param  {?Number} aHangMs Number of milliseconds that the hang should last.
 *                   Defaults to 0.
 * @return {String}  The data URI generated.
 */
function makeDataURI(aHangMs = 0) {
  return `data:text/html,
    <html>
      <head>
        <meta charset="utf-8"/>
        <title>Tab Spinner Test</title>
        <script>
          function hang() {
            let hangDuration = ${aHangMs};
            if (hangDuration > 0) {
              let startTime = window.performance.now();
              while(window.performance.now() - startTime < hangDuration) {}
            }
          }
        </script>
      </head>
      <body>
        <h1 id='header'>Tab Spinner Test</h1>
      </body>
    </html>`;
}

/**
 * Returns the sum of all values in an array.
 * @param  {Array}  aArray An array of integers
 * @return {Number} The sum of the integers in the array
 */
function sum(aArray) {
  return aArray.reduce(function(previousValue, currentValue) {
    return previousValue + currentValue;
  });
}

/**
 * A generator intended to be run as a Task. It tests one of the tab spinner
 * telemetry probes.
 * @param {String} aProbe The probe to test. Should be one of:
 *                  - FX_TAB_SWITCH_SPINNER_VISIBLE_MS
 *                  - FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS
 */
function* testProbe(aProbe) {
  info(`Testing probe: ${aProbe}`);
  let histogram = Services.telemetry.getHistogramById(aProbe);
  let buckets = histogram.snapshot().ranges.filter(function(value) {
    return (value > gMinHangTime && value < gMaxHangTime);
  });
  let delayTime = buckets[0]; // Pick a bucket arbitrarily

  // The tab spinner does not show up instantly. We need to hang for a little
  // bit of extra time to account for the tab spinner delay.
  delayTime += gBrowser.selectedTab.linkedBrowser.getTabBrowser()._getSwitcher().TAB_SWITCH_TIMEOUT;
  let dataURI1 = makeDataURI(delayTime);
  let dataURI2 = makeDataURI();

  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, dataURI1);
  histogram.clear();
  // Queue a hang in the content process when the
  // event loop breathes next.
  ContentTask.spawn(tab1.linkedBrowser, null, function*() {
    content.wrappedJSObject.hang();
  });
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, dataURI2);
  let snapshot = histogram.snapshot();
  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.removeTab(tab1);
  ok(sum(snapshot.counts) > 0,
   `Spinner probe should now have a value in some bucket`);
}

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount", 1]]
  });
});

add_task(testProbe.bind(null, "FX_TAB_SWITCH_SPINNER_VISIBLE_MS"));
add_task(testProbe.bind(null, "FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS"));
