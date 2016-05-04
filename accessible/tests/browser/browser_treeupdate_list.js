/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global ROLE_TEXT_LEAF, EVENT_REORDER, ROLE_STATICTEXT, ROLE_LISTITEM */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

function* setDisplayAndWaitForReorder(browser, value) {
  let onReorder = waitForEvent(EVENT_REORDER, 'ul');
  yield invokeSetStyle(browser, 'li', 'display', value);
  return yield onReorder;
}

addAccessibleTask(`
  <ul id="ul">
    <li id="li">item1</li>
  </ul>`, function*(browser, accDoc) {
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

  yield setDisplayAndWaitForReorder(browser, 'none');

  ok(isDefunct(li), 'Check that li is defunct.');
  ok(isDefunct(bullet), 'Check that bullet is defunct.');

  let event = yield setDisplayAndWaitForReorder(browser, 'list-item');

  testAccessibleTree(findAccessibleChildByID(event.accessible, 'li'), accTree);
});
