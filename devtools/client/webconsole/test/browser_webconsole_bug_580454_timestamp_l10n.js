/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that appropriately-localized timestamps are printed.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);
  const TEST_TIMESTAMP = 12345678;
  let date = new Date(TEST_TIMESTAMP);
  let localizedString = WCUL10n.timestampString(TEST_TIMESTAMP);
  isnot(localizedString.indexOf(date.getHours()), -1, "the localized " +
        "timestamp contains the hours");
  isnot(localizedString.indexOf(date.getMinutes()), -1, "the localized " +
        "timestamp contains the minutes");
  isnot(localizedString.indexOf(date.getSeconds()), -1, "the localized " +
        "timestamp contains the seconds");
  isnot(localizedString.indexOf(date.getMilliseconds()), -1, "the localized " +
        "timestamp contains the milliseconds");
});
