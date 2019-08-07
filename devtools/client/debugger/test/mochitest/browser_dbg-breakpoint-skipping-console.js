/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Test that debugger statements are skipped when
 * skip pausing is enabled.
 */

add_task(async function() {
  const toolbox = await initPane("doc-scripts.html", "webconsole", [
    ["devtools.debugger.skip-pausing", true]
  ]);
  toolbox.target.navigateTo({ url: "doc-debugger-statements.html" });

  await hasConsoleMessage({ toolbox }, 'done!')
  ok('We reached the end');
});
