/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that rejected promise are reported to the console.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
  <script>
    function createRejectedPromise(reason) {
      new Promise(function promiseCb(_, reject) {
        setTimeout(function setTimeoutCb(){
          reject(reason);
        }, 0);
      });
    }

    var err = new Error("carrot");
    err.name = "VeggieError";

    const reasons = [
      "potato",
      "",
      0,
      false,
      null,
      undefined,
      {fav: "eggplant"},
      ["cherry", "strawberry"],
      new Error("spinach"),
      err,
    ];

    reasons.forEach(function forEachCb(reason) {
      createRejectedPromise(reason);
    });
  </script>`;

add_task(async function() {
  await pushPref("javascript.options.asyncstack_capture_debuggee_only", false);
  const hud = await openNewTabAndConsole(TEST_URI);

  const expectedErrors = [
    "Uncaught (in promise) potato",
    "Uncaught (in promise) <empty string>",
    "Uncaught (in promise) 0",
    "Uncaught (in promise) false",
    "Uncaught (in promise) null",
    "Uncaught (in promise) undefined",
    `Uncaught (in promise) Object { fav: "eggplant" }`,
    `Uncaught (in promise) Array [ "cherry", "strawberry" ]`,
    `Uncaught (in promise) Error: spinach`,
    `Uncaught (in promise) VeggieError: carrot`,
  ];

  for (const expectedError of expectedErrors) {
    const message = await waitFor(
      () => findMessage(hud, expectedError, ".error"),
      `Couldn't find «${expectedError}» message`
    );
    ok(message, `Found «${expectedError}» message`);

    message.querySelector(".collapse-button").click();
    const framesEl = await waitFor(() => {
      const frames = message.querySelectorAll(
        ".message-body-wrapper > .stacktrace .frame"
      );
      return frames.length > 0 ? frames : null;
    }, "Couldn't find stacktrace");

    const frames = Array.from(framesEl)
      .map(frameEl =>
        Array.from(
          frameEl.querySelectorAll(".title, .location-async-cause")
        ).map(el => el.textContent.trim())
      )
      .flat();

    is(
      frames.join("\n"),
      [
        "setTimeoutCb",
        "(Async: setTimeout handler)",
        "promiseCb",
        "createRejectedPromise",
        "forEachCb",
        "forEach",
        "<anonymous>",
      ].join("\n"),
      "Error message has expected frames"
    );
  }
  ok(true, "All expected messages were found");

  info("Check that object in errors can be expanded");
  const rejectedObjectMessage = findMessage(
    hud,
    `Uncaught (in promise) Object { fav: "eggplant" }`,
    ".error"
  );
  const oi = rejectedObjectMessage.querySelector(".tree");
  ok(true, "The object was rendered in an ObjectInspector");

  info("Expanding the object");
  const onOiExpanded = waitFor(() => {
    return oi.querySelectorAll(".node").length === 3;
  });
  oi.querySelector(".arrow").click();
  await onOiExpanded;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "Object expanded"
  );

  // The object inspector now looks like:
  // Object { fav: "eggplant" }
  // |  fav: "eggplant"
  // |  <prototype>: Object { ... }

  const oiNodes = oi.querySelectorAll(".node");
  is(oiNodes.length, 3, "There is the expected number of nodes in the tree");

  ok(oiNodes[0].textContent.includes(`Object { fav: "eggplant" }`));
  ok(oiNodes[1].textContent.includes(`fav: "eggplant"`));
  ok(oiNodes[2].textContent.includes(`<prototype>: Object { \u2026 }`));
});
