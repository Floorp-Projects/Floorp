/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

async function setDisplayAndWaitForReorder(browser, value) {
  let onReorder = waitForEvent(EVENT_REORDER, 'ul');
  await invokeSetStyle(browser, 'li', 'display', value);
  return onReorder;
}

addAccessibleTask(`
  <ul id="ul">
    <li id="li">item1</li>
  </ul>`, async function(browser, accDoc) {
  let li = findAccessibleChildByID(accDoc, 'li');
  let bullet = li.firstChild;
  let accTree = {
    role: ROLE_LISTITEM,
    children: [ {
      role: ROLE_STATICTEXT,
      children: []
    }, {
      role: ROLE_TEXT_LEAF,
      children: []
    } ]
  };
  testAccessibleTree(li, accTree);

  await setDisplayAndWaitForReorder(browser, 'none');

  ok(isDefunct(li), 'Check that li is defunct.');
  ok(isDefunct(bullet), 'Check that bullet is defunct.');

  let event = await setDisplayAndWaitForReorder(browser, 'list-item');

  testAccessibleTree(findAccessibleChildByID(event.accessible, 'li'), accTree);
});
