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

add_task(async function () {
  await pushPref("devtools.source-map.client-service.enabled", true);

  const hud = await openNewTabAndConsole(PAGE_URL);
  const toolbox = hud.ui.wrapper.toolbox;

  info('Finding "here" message and waiting for source map to be applied');
  await waitFor(() => {
    const node = findConsoleAPIMessage(hud, "here");
    if (!node) {
      return false;
    }
    const messageLocationNode = node.querySelector(".message-location");
    const url = messageLocationNode.getAttribute("data-url");
    return url.includes("nosuchfile");
  });

  await testOpenInDebugger(hud, {
    text: "here",
    typeSelector: ".console-api",
    expectUrl: true,
    expectLine: false,
    expectColumn: false,
  });

  info("Selecting the console again");
  await toolbox.selectTool("webconsole");

  const node = await waitFor(() => findWarningMessage(hud, "original source"));
  ok(node, "source map error is displayed in web console");

  ok(
    !!node.querySelector(".learn-more-link"),
    "source map error has learn more link"
  );
});
