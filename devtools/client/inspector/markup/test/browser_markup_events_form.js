/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing the feature whereby custom registered actors can listen to
// 'form' events sent by the NodeActor to hook custom data to it.
// The test registers one backend actor providing custom form data
// and checks that the value is properly sent to the client (NodeFront).

const TEST_PAGE_URL = URL_ROOT + "doc_markup_events_form.html";
const TEST_ACTOR_URL = CHROME_URL_ROOT + "actor_events_form.js";

var {EventsFormFront} = require(TEST_ACTOR_URL);

add_task(function* () {
  info("Opening the Toolbox");
  let tab = yield addTab(TEST_PAGE_URL);
  let toolbox = yield openToolboxForTab(tab, "webconsole");

  info("Registering test actor");
  let {registrar, front} = yield registerTestActor(toolbox);

  info("Selecting the Inspector panel");
  let inspector = yield toolbox.selectTool("inspector");
  let container = yield getContainerForSelector("#container", inspector);
  isnot(container, null, "There must be requested container");

  let nodeFront = container.node;
  let value = nodeFront.getFormProperty("test-property");
  is(value, "test-value", "There must be custom property");

  info("Unregistering actor");
  yield unregisterActor(registrar, front);
});

function registerTestActor(toolbox) {
  let deferred = defer();

  let options = {
    prefix: "eventsFormActor",
    actorClass: "EventsFormActor",
    moduleUrl: TEST_ACTOR_URL,
  };

  // Register as a tab actor
  let client = toolbox.target.client;
  registerTabActor(client, options).then(({registrar, form}) => {
    // Attach to the registered actor
    let front = EventsFormFront(client, form);
    front.attach().then(() => {
      deferred.resolve({
        front: front,
        registrar: registrar,
      });
    });
  });

  return deferred.promise;
}
