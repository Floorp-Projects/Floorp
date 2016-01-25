/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that appropriately-localized timestamps are printed.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function*() {
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
