/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER, ROLE_TEXT_CONTAINER ROLE_PARAGRAPH, ROLE_TEXT_LEAF */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

function* removeTextData(browser, accessible, id, role) {
  let tree = {
    role: role,
    children: [ { role: ROLE_TEXT_LEAF, name: "text" } ]
  };
  testAccessibleTree(accessible, tree);

  let onReorder = waitForEvent(EVENT_REORDER, id);
  yield ContentTask.spawn(browser, id, id =>
    content.document.getElementById(id).firstChild.textContent = '');
  yield onReorder;

  tree = { role: role, children: [] };
  testAccessibleTree(accessible, tree);
}

addAccessibleTask(`
  <p id="p">text</p>
  <pre id="pre">text</pre>`, function*(browser, accDoc) {
  let p = findAccessibleChildByID(accDoc, 'p');
  let pre = findAccessibleChildByID(accDoc, 'pre');
  yield removeTextData(browser, p, 'p', ROLE_PARAGRAPH);
  yield removeTextData(browser, pre, 'pre', ROLE_TEXT_CONTAINER);
});
