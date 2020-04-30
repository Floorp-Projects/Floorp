/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("test event triggering actor creation", {
  async test(browser) {
    // Add a select element to the DOM of the loaded document.
    await SpecialPowers.spawn(browser, [], async function() {
      content.document.body.innerHTML += `
        <select id="testSelect">
          <option>A</option>
          <option>B</option>
        </select>`;
    });

    // Wait for the observer notification.
    let observePromise = new Promise(resolve => {
      const TOPIC = "test-js-window-actor-parent-event";
      Services.obs.addObserver(function obs(subject, topic, data) {
        is(topic, TOPIC, "topic matches");

        Services.obs.removeObserver(obs, TOPIC);
        resolve({ subject, data });
      }, TOPIC);
    });

    // Click on the select to show the dropdown.
    await BrowserTestUtils.synthesizeMouseAtCenter("#testSelect", {}, browser);

    // Wait for the observer notification to fire, and inspect the results.
    let { subject, data } = await observePromise;
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
