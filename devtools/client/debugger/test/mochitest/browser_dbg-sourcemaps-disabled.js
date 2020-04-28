/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests loading and pretty printing bundles with sourcemaps disabled
requestLongerTimeout(2);

add_task(async function() {
  await pushPref("devtools.source-map.client-service.enabled", false);
  const dbg = await initDebugger("doc-sourcemaps.html");

  await waitForSources(dbg, "bundle.js");
  const bundleSrc = findSource(dbg, "bundle.js");

  info('Pretty print the bundle');
  await selectSource(dbg, bundleSrc);
  clickElement(dbg, "prettyPrintButton");
  await waitForSelectedSource(dbg, "bundle.js:formatted");
  ok(true, 'everything finished');
});
