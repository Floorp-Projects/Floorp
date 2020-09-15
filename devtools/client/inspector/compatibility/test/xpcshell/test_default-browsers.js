/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test for the default browsers of user settings.

const {
  getDefaultTargetBrowsers,
} = require("devtools/client/inspector/shared/compatibility-user-settings");

add_task(() => {
  info("Check whether each default browsers data are unique by id and status");

  const defaultBrowsers = getDefaultTargetBrowsers();

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
