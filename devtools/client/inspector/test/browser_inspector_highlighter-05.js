/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// This is testing that the Anonymous Content is properly inserted into the document.
// Usually that is happening during the "interactive" state of the document, to have them
// ready as soon as possible.
// However, in some conditions, that's not possible since we don't have access yet to
// the `CustomContentContainer`, that is used to add the Anonymous Content.
// That can happen if the page has some external resource, as <link>, that takes time
// to load and / or returns the wrong content. This is not happening, for instance, with
// images.
//
// In those case, we want to be sure that if we're not able to insert the Anonymous
// Content at the "interactive" state, we're doing so when the document is loaded.
//
// See: https://bugzilla.mozilla.org/show_bug.cgi?id=1365075

const server = createTestHTTPServer();
const filepath = "/slow.css";
const cssuri = `http://localhost:${server.identity.primaryPort}${filepath}`;

// Register a slow css file handler so we can simulate a long loading time.
server.registerContentType("css", "text/css");
server.registerPathHandler(filepath, (metadata, response) => {
  info("CSS has been requested");
  response.processAsync();
  setTimeout(() => {
    info("CSS is responding");
    response.finish();
  }, 2000);
});

const TEST_URL = "data:text/html," + encodeURIComponent(`
  <!DOCTYPE html>
  <html>
    <head>
      <link href="${cssuri}" rel="stylesheet" />
    </head>
    <body>
      <p>Slow page</p>
     </body>
  </html>
`);

add_task(async function() {
  info("Open the inspector to a blank page.");
  const { inspector, tab, testActor } = await openInspectorForURL("about:blank");

  const pageLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("Navigate to the test url and waiting for the page to be loaded.");
  await navigateTo(inspector, TEST_URL);
  await pageLoaded;

  info("Shows the box model highligher for the <p> node.");
  const divFront = await getNodeFront("p", inspector);
  await inspector.highlighter.showBoxModel(divFront);

  info("Check the node is highlighted.");
  is(await testActor.isHighlighting(), true,
    "Box Model highlighter is working as expected.");
});
