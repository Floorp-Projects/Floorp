/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that an invalid source map is reported.

const JS_URL = URL_ROOT + "code_bundle_invalidmap.js";

const PAGE_URL = `data:text/html,
<!doctype html>

<html>
  <head>
    <meta charset="utf-8"/>
    <title>Empty test page to test source map with invalid source map</title>
  </head>

  <body>
    <script src="${JS_URL}"></script>
  </body>

</html>`;

add_task(async function() {
  await pushPref("devtools.source-map.client-service.enabled", true);

  const hud = await openNewTabAndConsole(PAGE_URL);

  const node = await waitFor(() => findMessage(hud, "Source map error"));
  ok(node, "source map error is displayed in web console");
});
