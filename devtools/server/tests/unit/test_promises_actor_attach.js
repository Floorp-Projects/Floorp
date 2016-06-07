/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can attach and detach to the PromisesActor under the correct
 * states.
 */

const { PromisesFront } = require("devtools/shared/fronts/promises");

add_task(function* () {
  let client = yield startTestDebuggerServer("promises-actor-test");
  let chromeActors = yield getChromeActors(client);

  // We have to attach the chrome TabActor before playing with the PromiseActor
  yield attachTab(client, chromeActors);
  yield testAttach(client, chromeActors);

  let response = yield listTabs(client);
  let targetTab = findTab(response.tabs, "promises-actor-test");
  ok(targetTab, "Found our target tab.");

  let [ tabResponse ] = yield attachTab(client, targetTab);

  yield testAttach(client, tabResponse);

  yield close(client);
});

function* testAttach(client, parent) {
  let promises = PromisesFront(client, parent);

  try {
    yield promises.detach();
    ok(false, "Should not be able to detach when in a detached state.");
  } catch (e) {
    ok(true, "Expected detach to fail when already in a detached state.");
  }

  yield promises.attach();
  ok(true, "Expected attach to succeed.");

  try {
    yield promises.attach();
    ok(false, "Should not be able to attach when in an attached state.");
  } catch (e) {
    ok(true, "Expected attach to fail when already in an attached state.");
  }

  yield promises.detach();
  ok(true, "Expected detach to succeed.");
}
