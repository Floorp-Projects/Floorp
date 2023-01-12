/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Load a page that generates multiple CSP parser warnings.

"use strict";

const TEST_FILE =
  "browser/devtools/client/webconsole/test/browser/test-warning-group-csp.html";

add_task(async function testCSPGroup() {
  const GROUP_LABEL = "Content-Security-Policy warnings";

  const hud = await openNewTabAndConsole("https://example.org/" + TEST_FILE);

  info("Checking for warning group");
  await checkConsoleOutputForWarningGroup(hud, [`▶︎⚠ ${GROUP_LABEL} 4`]);

  info("Expand the warning group");
  const node = findWarningMessage(hud, GROUP_LABEL);
  node.querySelector(".arrow").click();
  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${GROUP_LABEL} 4`,
    `| Ignoring “http:” within script-src: ‘strict-dynamic’ specified`,
    `| Ignoring “https:” within script-src: ‘strict-dynamic’ specified`,
    `| Ignoring “'unsafe-inline'” within script-src: ‘strict-dynamic’ specified`,
    `| Keyword ‘strict-dynamic’ within “script-src” with no valid nonce or hash might block all scripts from loading`,
  ]);
});
