/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the basic console.log()-style APIs and filtering work for sharedWorkers

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-workers.html";

function pushPrefEnv()
{
  let deferred = promise.defer();
  let options = {'set': [["dom.workers.sharedWorkers.enabled", true]]};
  SpecialPowers.pushPrefEnv(options, deferred.resolve);
  return deferred.promise;
}

let test = asyncTest(function*() {
  yield pushPrefEnv();
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  let outputNode = hud.outputNode;

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foo-bar-shared-worker"
    }],
  });

  hud.setFilterState('sharedworkers', false);

  is(outputNode.querySelectorAll(".filtered-by-type").length, 1,
     "1 message hidden for sharedworkers (logging turned off)")

  hud.setFilterState('sharedworkers', true);

  is(outputNode.querySelectorAll(".filtered-by-type").length, 0,
     "1 message shown for sharedworkers (logging turned off)")
});
