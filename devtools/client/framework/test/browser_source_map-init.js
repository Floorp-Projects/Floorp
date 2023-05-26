/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the source map service initializes properly when source
// actors have already been created.  Regression test for bug 1391768.

"use strict";

const JS_URL = URL_ROOT_SSL + "code_bundle_no_race.js";

const PAGE_URL = `data:text/html,
<!doctype html>

<html>
  <head>
    <meta charset="utf-8"/>
    <title>Empty test page to test race case</title>
  </head>

  <body>
    <script src="${JS_URL}"></script>
  </body>

</html>`;

const ORIGINAL_URL = "webpack:///code_no_race.js";

const GENERATED_LINE = 84;
const ORIGINAL_LINE = 11;

add_task(async function () {
  // Opening the debugger causes the source actors to be created.
  const toolbox = await openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  // In bug 1391768, when the sourceMapURLService was created, it was
  // ignoring any source actors that already existed, leading to
  // source-mapping failures for those.
  const service = toolbox.sourceMapURLService;

  info(`checking original location for ${JS_URL}:${GENERATED_LINE}`);
  const newLoc = await new Promise(r =>
    service.subscribeByURL(JS_URL, GENERATED_LINE, undefined, r)
  );
  is(newLoc.url, ORIGINAL_URL, "check mapped URL");
  is(newLoc.line, ORIGINAL_LINE, "check mapped line number");

  // See Bug 1637793 and Bug 1621337.
  // Ideally the debugger should only resolve when the worker targets have been
  // retrieved, which should be fixed by Bug 1621337 or a followup.
  info("Wait for all pending requests to settle on the DevToolsClient");
  await toolbox.commands.client.waitForRequestsToSettle();
});
