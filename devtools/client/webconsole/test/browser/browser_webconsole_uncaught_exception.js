/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that stack traces are shown when primitive values are thrown instead of
// error objects.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8>Test uncaught exception`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await checkThrowingWithStack(hud, `"tomato"`, "Uncaught tomato");
  await checkThrowingWithStack(hud, `""`, "Uncaught <empty string>");
  await checkThrowingWithStack(hud, `42`, "Uncaught 42");
  await checkThrowingWithStack(hud, `0`, "Uncaught 0");
  await checkThrowingWithStack(hud, `null`, "Uncaught null");
  await checkThrowingWithStack(hud, `undefined`, "Uncaught undefined");
  await checkThrowingWithStack(hud, `false`, "Uncaught false");

  await checkThrowingWithStack(
    hud,
    `new Error("watermelon")`,
    "Uncaught Error: watermelon"
  );

  await checkThrowingWithStack(
    hud,
    `(err = new Error("lettuce"), err.name = "VegetableError", err)`,
    "Uncaught VegetableError: lettuce"
  );

  await checkThrowingWithStack(
    hud,
    `{ fav: "eggplant" }`,
    `Uncaught Object { fav: "eggplant" }`
  );

  info("Check that object in errors can be expanded");
  const rejectedObjectMessage = findMessage(hud, "eggplant", ".error");
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

async function checkThrowingWithStack(hud, expression, expectedMessage) {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [expression], function(
    expr
  ) {
    const script = content.document.createElement("script");
    script.append(
      content.document.createTextNode(`
    a = () => {throw ${expr}};
    b =  () => a();
    c =  () => b();
    d =  () => c();
    d();
    `)
    );
    content.document.body.append(script);
    script.remove();
  });
  return checkMessageStack(hud, expectedMessage, [2, 3, 4, 5, 6]);
}
