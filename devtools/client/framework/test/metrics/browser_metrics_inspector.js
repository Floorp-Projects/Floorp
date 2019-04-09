/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test records the number of modules loaded by DevTools, as well as the total count
 * of characters in those modules, when opening the inspector. These metrics are retrieved
 * by perfherder via logs.
 */

const TEST_URL = "data:text/html;charset=UTF-8,<div>Inspector modules load test</div>";

add_task(async function() {
  await openNewTabAndToolbox(TEST_URL, "inspector");

  // The inspector does not use a dedicated browser loader.
  const loaders = [loader.provider.loader];

  runMetricsTest({
    filterString: "devtools/client/inspector",
    loaders,
    panelName: "inspector",
  });
});
