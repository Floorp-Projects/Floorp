/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the source map service doesn't race against source
// reporting.

"use strict";

const JS_URL = URL_ROOT + "code_bundle_no_race.js";

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

add_task(function* () {
  // Start with the empty page, then navigate, so that we can properly
  // listen for new sources arriving.
  const toolbox = yield openNewTabAndToolbox(PAGE_URL, "webconsole");
  const service = toolbox.sourceMapURLService;

  info(`checking original location for ${JS_URL}:${GENERATED_LINE}`);
  let newLoc = yield service.originalPositionFor(JS_URL, GENERATED_LINE);
  is(newLoc.sourceUrl, ORIGINAL_URL, "check mapped URL");
  is(newLoc.line, ORIGINAL_LINE, "check mapped line number");
});
