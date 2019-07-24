/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

function run_test() {
  const TEST_TIMESTAMP = 12345678;
  const WCUL10n = require("devtools/client/webconsole/utils/l10n");
  const date = new Date(TEST_TIMESTAMP);
  const localizedString = WCUL10n.timestampString(TEST_TIMESTAMP);
  ok(
    localizedString.includes(date.getHours()),
    "the localized timestamp contains the hours"
  );
  ok(
    localizedString.includes(date.getMinutes()),
    "the localized timestamp contains the minutes"
  );
  ok(
    localizedString.includes(date.getSeconds()),
    "the localized timestamp contains the seconds"
  );
  ok(
    localizedString.includes(date.getMilliseconds()),
    "the localized timestamp contains the milliseconds"
  );
}
