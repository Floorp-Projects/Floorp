/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that appropriately-localized timestamps are printed.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testTimestamp, false);

  function testTimestamp()
  {
    browser.removeEventListener("DOMContentLoaded", testTimestamp, false);
    const TEST_TIMESTAMP = 12345678;
    let date = new Date(TEST_TIMESTAMP);
    let localizedString = WCU_l10n.timestampString(TEST_TIMESTAMP);
    isnot(localizedString.indexOf(date.getHours()), -1, "the localized " +
          "timestamp contains the hours");
    isnot(localizedString.indexOf(date.getMinutes()), -1, "the localized " +
          "timestamp contains the minutes");
    isnot(localizedString.indexOf(date.getSeconds()), -1, "the localized " +
          "timestamp contains the seconds");
    isnot(localizedString.indexOf(date.getMilliseconds()), -1, "the localized " +
          "timestamp contains the milliseconds");
    finishTest();
  }
}

