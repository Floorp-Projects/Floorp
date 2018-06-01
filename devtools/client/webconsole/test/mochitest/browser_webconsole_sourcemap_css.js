/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a missing original source is reported.

const CSS_URL = URL_ROOT + "source-mapped.css";

const PAGE_URL = `data:text/html,
<!doctype html>

<html>
  <head>
    <meta charset="utf-8"/>
    <title>Empty test page to test source map and css</title>
  </head>

  <link href="${CSS_URL}" rel="stylesheet" type="text/css">
  <body>
    <div>
    There should be a source-mapped CSS warning in the console.
    </div>
  </body>

</html>`;

add_task(async function() {
  await pushPref("devtools.source-map.client-service.enabled", true);
  await pushPref("devtools.webconsole.filter.css", true);

  const hud = await openNewTabAndConsole(PAGE_URL);

  info("Waiting for css warning");
  const node = await waitFor(() => findMessage(hud, "octopus"));
  ok(!!node, "css warning seen");

  info("Waiting for source map to be applied");
  const found = await waitFor(() => {
    const frameLinkNode = node.querySelector(".message-location .frame-link");
    const url = frameLinkNode.getAttribute("data-url");
    return url.includes("scss");
  });

  ok(found, "css warning is source mapped in web console");
});
