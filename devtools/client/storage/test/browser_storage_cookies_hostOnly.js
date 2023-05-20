/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the HostOnly values displayed in the table are correct.

SpecialPowers.pushPrefEnv({
  set: [["security.allow_eval_with_system_principal", true]],
});

add_task(async function () {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-complex-values.html");

  gUI.tree.expandAll();

  showColumn("hostOnly", true);

  const c1id = getCookieId("c1", "test1.example.org", "/browser");
  await selectTableItem(c1id);
  checkCell(c1id, "hostOnly", "true");

  const c2id = getCookieId("cs2", ".example.org", "/");
  await selectTableItem(c2id);
  checkCell(c2id, "hostOnly", "false");
});
