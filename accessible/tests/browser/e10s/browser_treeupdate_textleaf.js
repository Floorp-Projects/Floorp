/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

async function removeTextData(browser, accessible, id, role) {
  let tree = {
    role,
    children: [ { role: ROLE_TEXT_LEAF, name: "text" } ]
  };
  testAccessibleTree(accessible, tree);

  let onReorder = waitForEvent(EVENT_REORDER, id);
  await ContentTask.spawn(browser, id, contentId => {
    content.document.getElementById(contentId).firstChild.textContent = "";
  });
  await onReorder;

  tree = { role, children: [] };
  testAccessibleTree(accessible, tree);
}

addAccessibleTask(`
  <p id="p">text</p>
  <pre id="pre">text</pre>`, async function(browser, accDoc) {
  let p = findAccessibleChildByID(accDoc, "p");
  let pre = findAccessibleChildByID(accDoc, "pre");
  await removeTextData(browser, p, "p", ROLE_PARAGRAPH);
  await removeTextData(browser, pre, "pre", ROLE_TEXT_CONTAINER);
});
