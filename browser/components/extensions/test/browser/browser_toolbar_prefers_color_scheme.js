/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testThemeDeterminesToolbarQuery() {
  let darkModeQuery = window.matchMedia("(prefers-color-scheme: dark)");
  let darkToolbarQuery = window.matchMedia(
    "(-moz-toolbar-prefers-color-scheme: dark)"
  );

  let darkExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "dark@mochi.test",
        },
      },
      theme: {
        colors: {
          toolbar: "rgba(12, 12, 12, 1)",
        },
      },
    },
  });
  let lightExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "light@mochi.test",
        },
      },
      theme: {
        colors: {
          toolbar: "rgba(255, 255, 255, 1)",
        },
      },
    },
  });

  let initialDarkModeMatches = darkModeQuery.matches;
  ok(
    initialDarkModeMatches == darkToolbarQuery.matches,
    "OS dark mode matches toolbar initially"
  );

  let testExtensions = darkToolbarQuery.matches
    ? [lightExtension, darkExtension]
    : [darkExtension, lightExtension];

  await Promise.all([
    BrowserTestUtils.waitForEvent(darkToolbarQuery, "change"),
    TestUtils.topicObserved("lightweight-theme-styling-update"),
    testExtensions[0].startup(),
  ]);

  is(darkModeQuery.matches, initialDarkModeMatches, "OS dark mode unchanged");
  is(
    darkToolbarQuery.matches,
    !initialDarkModeMatches,
    "toolbar query changed"
  );

  await Promise.all([
    BrowserTestUtils.waitForEvent(darkToolbarQuery, "change"),
    TestUtils.topicObserved("lightweight-theme-styling-update"),
    testExtensions[1].startup(),
  ]);

  is(darkModeQuery.matches, initialDarkModeMatches, "OS dark mode unchanged");
  is(
    darkToolbarQuery.matches,
    initialDarkModeMatches,
    "toolbar query matches OS"
  );

  await lightExtension.unload();
  await darkExtension.unload();
});
