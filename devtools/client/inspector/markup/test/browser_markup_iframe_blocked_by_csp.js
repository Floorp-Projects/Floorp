/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that iframe blocked because of CSP doesn't cause the browser to freeze.

const TEST_URI = `https://example.com/document-builder.sjs?html=${encodeURIComponent(`
  <h1>Test expanding CSP-blocked iframe</h1>
  <iframe src="https://example.org/document-builder.sjs?html=Hello"></iframe>
`)}&headers=content-security-policy:default-src 'self'`;

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URI);
  const container = await getContainerForSelector("iframe", inspector);
  await expandContainer(inspector, container);

  const h1Container = await getContainerForSelector("h1", inspector);
  ok(
    h1Container.elt.isConnected,
    "Inspector panel did not freeze/crash when expanding the blocked-by-csp iframe"
  );
});
