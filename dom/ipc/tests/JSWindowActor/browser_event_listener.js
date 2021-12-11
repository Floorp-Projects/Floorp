/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

async function createAndShowDropdown(browser) {
  // Add a select element to the DOM of the loaded document.
  await SpecialPowers.spawn(browser, [], async function() {
    content.document.body.innerHTML += `
      <select id="testSelect">
        <option>A</option>
        <option>B</option>
      </select>`;
  });

  // Click on the select to show the dropdown.
  await BrowserTestUtils.synthesizeMouseAtCenter("#testSelect", {}, browser);
}

declTest("test event triggering actor creation", {
  events: { mozshowdropdown: {} },

  async test(browser) {
    // Wait for the observer notification.
    let observePromise = TestUtils.topicObserved(
      "test-js-window-actor-parent-event"
    );

    await createAndShowDropdown(browser);

    // Wait for the observer notification to fire, and inspect the results.
    let [subject, data] = await observePromise;
    is(data, "mozshowdropdown");

    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("TestWindow");
    ok(actorParent, "JSWindowActorParent should have value.");
    is(
      subject.wrappedJSObject,
      actorParent,
      "Should have been recieved on the right actor"
    );
  },
});

declTest("test createActor:false not triggering actor creation", {
  events: { mozshowdropdown: { createActor: false } },

  async test(browser) {
    info("before actor has been created");
    const TOPIC = "test-js-window-actor-parent-event";
    function obs() {
      ok(false, "actor should not be created");
    }
    Services.obs.addObserver(obs, TOPIC);

    await createAndShowDropdown(browser);

    // Bounce into the content process & create the actor after showing the
    // dropdown, in order to be sure that if an event was going to be
    // delivered, it already would've been.
    await SpecialPowers.spawn(browser, [], async () => {
      content.windowGlobalChild.getActor("TestWindow");
    });
    ok(true, "observer notification should not have fired");
    Services.obs.removeObserver(obs, TOPIC);

    // ----------------------------------------------------
    info("after actor has been created");
    let observePromise = TestUtils.topicObserved(
      "test-js-window-actor-parent-event"
    );
    await createAndShowDropdown(browser);

    // Wait for the observer notification to fire, and inspect the results.
    let [subject, data] = await observePromise;
    is(data, "mozshowdropdown");

    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("TestWindow");
    ok(actorParent, "JSWindowActorParent should have value.");
    is(
      subject.wrappedJSObject,
      actorParent,
      "Should have been recieved on the right actor"
    );
  },
});
