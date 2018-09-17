/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a missing original source is reported.

const JS_URL = URL_ROOT + "code_bundle_nosource.js";

const PAGE_URL = `data:text/html,
<!doctype html>

<html>
  <head>
    <meta charset="utf-8"/>
    <title>Empty test page to test source map with missing original source</title>
  </head>

  <body>
    <script src="${JS_URL}"></script>
  </body>

</html>`;

add_task(async function() {
  // Force the new debugger UI, in case this gets uplifted with the old
  // debugger still turned on
  await pushPref("devtools.debugger.new-debugger-frontend", true);
  await pushPref("devtools.source-map.client-service.enabled", true);

  const hud = await openNewTabAndConsole(PAGE_URL);
  const toolbox = hud.ui.consoleOutput.toolbox;

  info("Finding \"here\" message and waiting for source map to be applied");
  await waitFor(() => {
    const node = findMessage(hud, "here");
    if (!node) {
      return false;
    }
    const frameLinkNode = node.querySelector(".message-location .frame-link");
    const url = frameLinkNode.getAttribute("data-url");
    return url.includes("nosuchfile");
  });

  await testOpenInDebugger(hud, toolbox, "here");

  info("Selecting the console again");
  await toolbox.selectTool("webconsole");

  const node = await waitFor(() => findMessage(hud, "original source"));
  ok(node, "source map error is displayed in web console");

  ok(!!node.querySelector(".learn-more-link"), "source map error has learn more link");
});
