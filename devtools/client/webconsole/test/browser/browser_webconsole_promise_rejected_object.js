/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that rejected promise are reported to the console.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
  <script>
    new Promise((_, reject) => setTimeout(reject, 0, "potato"));
    new Promise((_, reject) => setTimeout(reject, 0, ""));
    new Promise((_, reject) => setTimeout(reject, 0, 0));
    new Promise((_, reject) => setTimeout(reject, 0, false));
    new Promise((_, reject) => setTimeout(reject, 0, null));
    new Promise((_, reject) => setTimeout(reject, 0, undefined));
    new Promise((_, reject) => setTimeout(reject, 0, {fav: "eggplant"}));
    new Promise((_, reject) => setTimeout(reject, 0, ["cherry", "strawberry"]));
    new Promise((_, reject) => setTimeout(reject, 0, new Error("spinach")));
    var err = new Error("carrot");
    err.name = "VeggieError";
    new Promise((_, reject) => setTimeout(reject, 0, err));
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const expectedErrors = [
    "Uncaught potato",
    "Uncaught <empty string>",
    "Uncaught 0",
    "Uncaught false",
    "Uncaught null",
    "Uncaught undefined",
    `Uncaught Object { fav: "eggplant" }`,
    `Uncaught Array [ "cherry", "strawberry" ]`,
    `Uncaught Error: spinach`,
    `Uncaught VeggieError: carrot`,
  ];

  for (const expectedError of expectedErrors) {
    await waitFor(
      () => findMessage(hud, expectedError, ".error"),
      `Couldn't find «${expectedError}» message`
    );
    ok(true, `Found «${expectedError}» message`);
  }
  ok(true, "All expected messages were found");

  info("Check that object in errors can be expanded");
  const rejectedObjectMessage = findMessage(
    hud,
    `Uncaught Object { fav: "eggplant" }`,
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
  // {...}
  // |  fav: "eggplant"
  // |  <prototype>: Object { ... }

  const oiNodes = oi.querySelectorAll(".node");
  is(oiNodes.length, 3, "There is the expected number of nodes in the tree");

  ok(oiNodes[0].textContent.includes(`{\u2026}`));
  ok(oiNodes[1].textContent.includes(`fav: "eggplant"`));
  ok(oiNodes[2].textContent.includes(`<prototype>: Object { \u2026 }`));
});
