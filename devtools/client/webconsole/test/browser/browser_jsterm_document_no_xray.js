/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html,Test evaluating document";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  // check for occurrences of Object XRayWrapper, bug 604430
  const { node } = await executeAndWaitForMessage(
    hud,
    "document",
    "HTMLDocument",
    ".result"
  );
  is(node.textContent.includes("xray"), false, "document - no XrayWrapper");
});
