/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that getter properties that return long strings can be expanded. See Bug 1481833.

"use strict";

const LONGSTRING = "a ".repeat(10000);
const TEST_URI = `data:text/html,Test expanding longString getter property
  <svg>
    <image xlink:href="data:image/png;base64,${LONGSTRING}"></image>
  </svg>
  <script>
    console.dir("Test message", document.querySelector("svg image").href);
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  // Retrieve the logged message.
  const message = await waitFor(() => findMessage(hud, "Test message"));

  // Wait until the SVGAnimatedString is expanded.
  await waitFor(() => message.querySelectorAll(".arrow").length > 1);

  const arrow = message.querySelectorAll(".arrow")[1];
  ok(arrow, "longString expand arrow is shown");

  info("wait for long string expansion");
  const onLongStringFullTextDisplayed = waitFor(() =>
    findMessage(hud, LONGSTRING)
  );
  arrow.click();
  await onLongStringFullTextDisplayed;

  ok(true, "The full text of the longString is displayed");

  info("wait for long string collapse");
  const onLongStringCollapsed = waitFor(() => !findMessage(hud, LONGSTRING));
  arrow.click();
  await onLongStringCollapsed;

  ok(true, "The longString can be collapsed");
});
