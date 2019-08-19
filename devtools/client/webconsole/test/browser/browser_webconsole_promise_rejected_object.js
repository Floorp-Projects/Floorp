/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that rejected non-error objects are reported to the console.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
  <script>
    const v = [1,2,3];
    new Promise((resolve, reject) => setTimeout(reject, 0, v));
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() => findMessage(hud, "Object"));
  ok(true, "Message displayed for rejected object");
});
