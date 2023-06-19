/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that the filter value input works in the sidebar.

"use strict";

add_task(async function () {
  await openTabAndSetupStorage(
    MAIN_DOMAIN_SECURED + "storage-complex-values.html"
  );

  const updated = gUI.once("sidebar-updated");
  await selectTreeItem(["localStorage", "https://test1.example.org"]);
  await selectTableItem("ls1");
  await updated;

  const doc = gPanelWindow.document;

  let properties = doc.querySelectorAll(`.variables-view-property`);
  let unmatched = doc.querySelectorAll(`.variables-view-property[unmatched]`);
  is(properties.length, 5, "5 properties in total before filtering");
  is(unmatched.length, 0, "No unmatched properties before filtering");

  info("Focus the filter input and type 'es6' to filter out entries");
  doc.querySelector(".variables-view-searchinput").focus();
  EventUtils.synthesizeKey("es6", {}, gPanelWindow);

  await waitFor(
    () =>
      doc.querySelectorAll(`.variables-view-property[unmatched]`).length == 3
  );

  info("Updates performed, going to verify result");
  properties = doc.querySelectorAll(`.variables-view-property`);
  unmatched = doc.querySelectorAll(`.variables-view-property[unmatched]`);
  is(properties.length, 5, "5 properties in total after filtering");
  // Note: even though only one entry matches, since the VariablesView displays
  // it as a tree, we also have one matched .variables-view-property for the
  // parent.
  is(unmatched.length, 3, "Three unmatched properties after filtering");
});
