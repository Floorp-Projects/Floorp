/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const HIST_ID = "USE_COUNTER2_JS_LATE_WEEKDAY_PAGE";

const triggers = [
  "Sep 26 Tues 1995",
  "Sep 26 1995 Tues",
  "Sep 26 1995 Tues 09:30",
  "Sep 26 1995 09:Tues:30",
  "Sep 26 1995 09:30 Tues GMT",
  "Sep 26 1995 09:30 GMT Tues",

  "26 Tues Sep 1995",
  "26 Sep Tues 1995",
  "26 Sep 1995 Tues",

  "1995-09-26 Tues",

  // Multiple occurences should only trigger 1 counter
  "Sep 26 Tues 1995 Tues",
];
const nonTriggers = [
  "Sep 26 1995",
  "Tues Sep 26 1995",
  "Sep Tues 26 1995",

  // Invalid format shouldn't trigger the counter
  "Sep 26 Tues 1995 foo",
];

function getCount() {
  return Services.telemetry.getHistogramById(HIST_ID).snapshot().sum;
}

/**
 * Opens and closes a browser tab with minimal JS code which parses
 * the given Date format.
 */
async function parseFormat(format, call = "new Date") {
  let newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `data:text/html;charset=utf-8,<script>${call}("${format}")</script>`
  );
  BrowserTestUtils.removeTab(newTab);
}

add_task(async function test_date_telemetry() {
  let sum = getCount();

  // waitForCondition cannot be used to test if nothing has changed,
  // so these tests aren't as reliable as the ones in the next loop.
  // If you encounter an inexplicable failure in any of these tests,
  // debug by adding a delay to the end of the parseFormat function.
  for (const format of nonTriggers) {
    await parseFormat(format);
    const count = getCount();
    is(count, sum, `${format} should not trigger telemetry`);
    sum = count;
  }

  for (const [i, format] of triggers.entries()) {
    // Alternate between Date constructor and Date.parse
    await parseFormat(format, ["new Date", "Date.parse"][i % 2]);
    await BrowserTestUtils.waitForCondition(() => getCount() > sum);
    const count = getCount();
    is(count, sum + 1, `${format} should trigger telemetry`);
    sum = count;
  }
});
