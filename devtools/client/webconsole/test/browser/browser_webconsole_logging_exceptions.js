/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that logging exceptions works as expected

"use strict";

const TEST_URI =
  `data:text/html;charset=utf8,` +
  encodeURI(`<!DOCTYPE html><script>
  const domExceptionOnLine2 = new DOMException("Bar");
  /* console.error will be on line 4 */
  console.error("Foo", domExceptionOnLine2);
</script>`);

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Wait for the error to be logged");
  const msgNode = await waitFor(() => findConsoleAPIMessage(hud, "Foo"));
  ok(!msgNode.classList.contains("open"), `Error logged not expanded`);

  const framesNode = await waitFor(() => msgNode.querySelector(".pane.frames"));
  ok(framesNode, "The DOMException stack is displayed right away");

  const frameNodes = framesNode.querySelectorAll(".frame");
  is(frameNodes.length, 1, "Expected frames are displayed");
  is(
    frameNodes[0].querySelector(".line").textContent,
    "2",
    "The stack displayed by default refers to second argument passed to console.error and refers to DOMException callsite"
  );

  info(
    "Check that the console.error stack is refering to console.error() callsite"
  );
  await checkMessageStack(hud, "Foo", [4]);
});
