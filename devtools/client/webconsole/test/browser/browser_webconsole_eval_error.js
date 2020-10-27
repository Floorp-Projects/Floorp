/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that throwing uncaught errors while doing console evaluations shows the expected
// error message, with or without stack, in the console.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-eval-error.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  execute(hud, "throwErrorObject()");
  await checkMessageStack(hud, "ThrowErrorObject", [6, 1]);

  execute(hud, "throwValue(40 + 2)");
  await checkMessageStack(hud, "42", [14, 10, 1]);

  await checkThrowingEvaluationWithStack(hud, `"bloop"`, "Uncaught bloop");
  await checkThrowingEvaluationWithStack(hud, `""`, "Uncaught <empty string>");
  await checkThrowingEvaluationWithStack(hud, `0`, "Uncaught 0");
  await checkThrowingEvaluationWithStack(hud, `null`, "Uncaught null");
  await checkThrowingEvaluationWithStack(
    hud,
    `undefined`,
    "Uncaught undefined"
  );
  await checkThrowingEvaluationWithStack(hud, `false`, "Uncaught false");

  await checkThrowingEvaluationWithStack(
    hud,
    `new Error("watermelon")`,
    "Uncaught Error: watermelon"
  );

  await checkThrowingEvaluationWithStack(
    hud,
    `(err = new Error("lettuce"), err.name = "VegetableError", err)`,
    "Uncaught VegetableError: lettuce"
  );

  await checkThrowingEvaluationWithStack(
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

  execute(hud, `1 + @`);
  const messageNode = await waitFor(() =>
    findMessage(hud, "illegal character")
  );
  is(
    messageNode.querySelector(".frames"),
    null,
    "There's no stacktrace for a SyntaxError evaluation"
  );
});

function checkThrowingEvaluationWithStack(hud, expression, expectedMessage) {
  execute(
    hud,
    `
    a = () => {throw ${expression}};
    b =  () => a();
    c =  () => b();
    d =  () => c();
    d();
  `
  );
  return checkMessageStack(hud, expectedMessage, [2, 3, 4, 5, 6]);
}
