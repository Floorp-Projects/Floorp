/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the source map service can fetch a source map from a
// different domain.

"use strict";

const JS_URL = URL_ROOT + "code_bundle_cross_domain.js";

const PAGE_URL = `data:text/html,
<!doctype html>

<html>
  <head>
    <meta charset="utf-8"/>
    <title>Empty test page to test cross domain source map</title>
  </head>

  <body>
    <script src="${JS_URL}"></script>
  </body>

</html>`;

const ORIGINAL_URL = "webpack:///code_cross_domain.js";

const GENERATED_LINE = 82;
const ORIGINAL_LINE = 12;

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(PAGE_URL, "webconsole");
  const service = toolbox.sourceMapURLService;

  info(`checking original location for ${JS_URL}:${GENERATED_LINE}`);
  const newLoc = await service.originalPositionFor(JS_URL, GENERATED_LINE);
  is(newLoc.sourceUrl, ORIGINAL_URL, "check mapped URL");
  is(newLoc.line, ORIGINAL_LINE, "check mapped line number");
});
