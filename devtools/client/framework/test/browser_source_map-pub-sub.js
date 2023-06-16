/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the source map service subscribe mechanism work as expected.

"use strict";

const JS_URL = URL_ROOT + "code_bundle_no_race.js";

const PAGE_URL = `data:text/html,
<!doctype html>
<html>
  <head>
    <meta charset="utf-8"/>
  </head>
  <body>
    <script src="${JS_URL}"></script>
  </body>
</html>`;

const ORIGINAL_URL = "webpack:///code_no_race.js";

const SOURCE_MAP_PREF = "devtools.source-map.client-service.enabled";

const GENERATED_LINE = 84;
const ORIGINAL_LINE = 11;

add_task(async function () {
  // Push a pref env so any changes will be reset at the end of the test.
  await SpecialPowers.pushPrefEnv({});

  // Opening the debugger causes the source actors to be created.
  const toolbox = await openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  const service = toolbox.sourceMapURLService;

  const cbCalls = [];
  const cb = originalLocation => cbCalls.push(originalLocation);
  const expectedArg = { url: ORIGINAL_URL, line: ORIGINAL_LINE, column: 0 };

  // Wait for the sources to fully populate so that waitForSubscriptionsToSettle
  // can be guaranteed that all actions have been queued.
  await service._ensureAllSourcesPopulated();

  const unsubscribe1 = service.subscribeByURL(JS_URL, GENERATED_LINE, 1, cb);

  // Wait for the query to finish and populate so that all of the later
  // logic with this position will run synchronously, and the subscribe has run.
  for (const map of service._mapsById.values()) {
    for (const query of map.queries.values()) {
      await query.action;
    }
  }

  is(
    cbCalls.length,
    1,
    "The callback function is called directly when subscribing"
  );
  Assert.deepEqual(
    cbCalls[0],
    expectedArg,
    "callback called with expected arguments"
  );

  const unsubscribe2 = service.subscribeByURL(JS_URL, GENERATED_LINE, 1, cb);
  is(cbCalls.length, 2, "Subscribing to the same location twice works");
  Assert.deepEqual(
    cbCalls[1],
    expectedArg,
    "callback called with expected arguments"
  );

  info("Manually call the dispatcher to ensure subscribers are called");
  Services.prefs.setBoolPref(SOURCE_MAP_PREF, false);
  is(cbCalls.length, 4, "both subscribers were called");
  Assert.deepEqual(cbCalls[2], null, "callback called with expected arguments");
  Assert.deepEqual(
    cbCalls[2],
    cbCalls[3],
    "callbacks were passed the same arguments"
  );

  info("Check unsubscribe functions");
  unsubscribe1();
  Services.prefs.setBoolPref(SOURCE_MAP_PREF, true);
  is(cbCalls.length, 5, "Only remainer subscriber callback was called");
  Assert.deepEqual(
    cbCalls[4],
    expectedArg,
    "callback called with expected arguments"
  );

  unsubscribe2();
  Services.prefs.setBoolPref(SOURCE_MAP_PREF, false);
  Services.prefs.setBoolPref(SOURCE_MAP_PREF, true);
  is(cbCalls.length, 5, "No callbacks were called");
});
