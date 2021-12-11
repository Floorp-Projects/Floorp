/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the adding of sessionStorage entries.

"use strict";

add_task(async function() {
  await openTabAndSetupStorage(
    MAIN_DOMAIN_SECURED + "storage-sessionstorage.html"
  );
  showAllColumns(true);

  await performAdd(["sessionStorage", "https://test1.example.org"]);
  await performAdd(["sessionStorage", "https://test1.example.org"]);
  await performAdd(["sessionStorage", "https://test1.example.org"]);
  await performAdd(["sessionStorage", "https://test1.example.org"]);
  await performAdd(["sessionStorage", "https://test1.example.org"]);
});
