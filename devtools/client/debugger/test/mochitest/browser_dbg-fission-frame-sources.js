/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const TEST_COM_URI =
  URL_ROOT_COM + "examples/doc_dbg-fission-frame-sources.html";

add_task(async function() {
  // Simply load a test page with a remote frame and wait for both sources to
  // be visible.
  // simple1.js is imported by the main page. simple2.js comes from the frame.
  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_COM_URI,
    "simple1.js",
    "simple2.js"
  );

  const rootNodes = dbg.win.document.querySelectorAll(
    selectors.sourceTreeRootNode
  );

  info("Expands the main root node");
  await expandAllSourceNodes(dbg, rootNodes[0]);

  // We need to assert the actual DOM nodes in the source tree, because the
  // state can contain simple1.js and simple2.js, but only show one of them.
  info("Waiting for simple1.js from example.com (parent page)");
  await waitUntil(() => findSourceNodeWithText(dbg, "simple1.js"));

  // If fission is enabled, the second source is under another root node.
  if (isFissionEnabled()) {
    is(rootNodes.length, 2, "Found 2 sourceview root nodes when fission is on");

    info("Expands the remote frame root node");
    await expandAllSourceNodes(dbg, rootNodes[1]);
  }

  info("Waiting for simple2.js from example.org (frame)");
  await waitUntil(() => findSourceNodeWithText(dbg, "simple2.js"));

  await dbg.toolbox.closeToolbox();
});
