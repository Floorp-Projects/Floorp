/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test for the default browsers of user settings.

const {
  getBrowsersList,
} = require("resource://devtools/shared/compatibility/compatibility-user-settings.js");

add_task(async () => {
  info("Check whether each default browsers data are unique by id and status");

  const defaultBrowsers = await getBrowsersList();

  for (const target of defaultBrowsers) {
    const count = defaultBrowsers.reduce(
      (currentCount, browser) =>
        target.id === browser.id && target.status === browser.status
          ? currentCount + 1
          : currentCount,
      0
    );

    equal(count, 1, `This browser (${target.id} - ${target.status}) is unique`);
  }
});
