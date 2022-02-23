/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that iframe blocked because of CSP doesn't cause the browser to freeze.

const TEST_URI = `https://example.com/document-builder.sjs?html=${encodeURIComponent(`
  <h1>Test expanding CSP-blocked iframe</h1>
  <iframe src="https://example.org/document-builder.sjs?html=Hello"></iframe>
`)}&headers=content-security-policy:default-src 'self'`;

const BYPASS_WALKERFRONT_CHILDREN_IFRAME_GUARD_PREF =
  "devtools.testing.bypass-walker-children-iframe-guard";

add_task(async function() {
  await pushPref(BYPASS_WALKERFRONT_CHILDREN_IFRAME_GUARD_PREF, true);
  const { inspector } = await openInspectorForURL(TEST_URI);
  let container = await getContainerForSelector("iframe", inspector);
  is(
    container.expander.style.visibility,
    "hidden",
    "Expand icon is hidden, even without the safe guard in WalkerFront#children"
  );

  info("Reload the page and do same assertion with the guard");
  Services.prefs.clearUserPref(BYPASS_WALKERFRONT_CHILDREN_IFRAME_GUARD_PREF);
  await reloadBrowser();
  container = await getContainerForSelector("iframe", inspector);
  is(
    container.expander.style.visibility,
    "hidden",
    "Expand icon is still hidden"
  );
});
