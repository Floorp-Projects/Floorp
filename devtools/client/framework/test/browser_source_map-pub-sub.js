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

const GENERATED_LINE = 84;
const ORIGINAL_LINE = 11;

add_task(async function() {
  // Opening the debugger causes the source actors to be created.
  const toolbox = await openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  const service = toolbox.sourceMapURLService;

  const cbCalls = [];
  const cb = (...args) => cbCalls.push(args);
  const expectedArgs = [true, ORIGINAL_URL, ORIGINAL_LINE, 0];

  const unsubscribe1 = service.subscribe(JS_URL, GENERATED_LINE, 1, cb);
  await waitForSubscribtionsPromise(service);
  is(
    cbCalls.length,
    1,
    "The callback function is called directly when subscribing"
  );
  Assert.deepEqual(
    cbCalls[0],
    expectedArgs,
    "callback called with expected arguments"
  );

  const unsubscribe2 = service.subscribe(JS_URL, GENERATED_LINE, 1, cb);
  await waitForSubscribtionsPromise(service);
  is(cbCalls.length, 2, "Subscribing to the same location twice works");
  Assert.deepEqual(
    cbCalls[1],
    expectedArgs,
    "callback called with expected arguments"
  );

  info("Manually call the dispatcher to ensure subscribers are called");
  service._dispatchSubscribersForURL(JS_URL);
  await waitForSubscribtionsPromise(service);
  is(cbCalls.length, 4, "both subscribers were called");
  Assert.deepEqual(
    cbCalls[2],
    expectedArgs,
    "callback called with expected arguments"
  );
  Assert.deepEqual(
    cbCalls[2],
    cbCalls[3],
    "callbacks were passed the same arguments"
  );

  info("Check unsubscribe functions");
  unsubscribe1();
  service._dispatchSubscribersForURL(JS_URL);
  await waitForSubscribtionsPromise(service);
  is(cbCalls.length, 5, "Only remainer subscriber callback was called");
  Assert.deepEqual(
    cbCalls[4],
    expectedArgs,
    "callback called with expected arguments"
  );

  unsubscribe2();
  service._dispatchSubscribersForURL(JS_URL);
  await waitForSubscribtionsPromise(service);
  is(cbCalls.length, 5, "No callbacks were called");
});

async function waitForSubscribtionsPromise(service) {
  for (const [, subscriptionEntry] of service._subscriptions) {
    if (subscriptionEntry.promise) {
      await subscriptionEntry.promise;
    }
  }
}
