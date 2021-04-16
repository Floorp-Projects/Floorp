/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testThemeDeterminesToolbarQuery() {
  let darkModeQuery = window.matchMedia("(prefers-color-scheme: dark)");
  let darkToolbarQuery = window.matchMedia(
    "(-moz-toolbar-prefers-color-scheme: dark)"
  );

  let initialDarkModeMatches = darkModeQuery.matches;

  let darkExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          toolbar: "rgba(12, 12, 12, 1)",
        },
      },
    },
  });
  let lightExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          toolbar: "rgba(255, 255, 255, 1)",
        },
      },
    },
  });

  await Promise.all([
    BrowserTestUtils.waitForEvent(darkToolbarQuery, "change"),
    TestUtils.topicObserved("lightweight-theme-styling-update"),
    darkExtension.startup(),
  ]);

  is(darkModeQuery.matches, initialDarkModeMatches, "OS dark mode unchanged");
  ok(darkToolbarQuery.matches, "toolbar query is dark mode");

  await Promise.all([
    BrowserTestUtils.waitForEvent(darkToolbarQuery, "change"),
    TestUtils.topicObserved("lightweight-theme-styling-update"),
    lightExtension.startup(),
  ]);

  is(darkModeQuery.matches, initialDarkModeMatches, "OS dark mode unchanged");
  ok(!darkToolbarQuery.matches, "toolbar query is light mode");

  await lightExtension.unload();
  await darkExtension.unload();
});
