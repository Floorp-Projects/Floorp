/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */
"use strict";

add_task(async function() {
  const URL1 = buildURLWithContent(
    "example.com",
    `<h1>example.com</h1>` + `<script>document.cookie = "lorem=ipsum";</script>`
  );
  const URL2 = buildURLWithContent(
    "example.net",
    `<h1>example.net</h1>` + `<script>document.cookie = "foo=bar";</script>`
  );

  // open tab
  await openTabAndSetupStorage(URL1);
  const doc = gPanelWindow.document;

  // Check first domain
  // check that both host appear in the storage tree
  checkTree(doc, ["cookies", "http://example.com"]);
  // check the table for values
  await selectTreeItem(["cookies", "http://example.com"]);
  checkCookieData("lorem", "ipsum");

  // NOTE: No need to clean up cookies since Services.cookies.removeAll() from
  // the registered clean up function will remove all of them.

  // Check second domain
  await navigateTo(URL2);
  // wait for storage tree refresh, and check host
  info("Waiting for storage tree to refresh and show correct host…");
  await waitUntil(() => isInTree(doc, ["cookies", "http://example.net"]));
  // check the table for values
  await selectTreeItem(["cookies", "http://example.net"]);
  checkCookieData("foo", "bar");
});
