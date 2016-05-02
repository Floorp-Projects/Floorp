/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

function* setHidden(browser, value) {
  let onReorder = waitForEvent(EVENT_REORDER, 'container');
  yield invokeSetAttribute(browser, 'child', 'hidden', value);
  yield onReorder;
}

addAccessibleTask('<div id="container"><input id="child"></div>',
  function*(browser, accDoc) {
    let container = findAccessibleChildByID(accDoc, 'container');

    testAccessibleTree(container, { SECTION: [ { ENTRY: [ ] } ] });

    // Set @hidden attribute
    yield setHidden(browser, 'true');
    testAccessibleTree(container, { SECTION: [ ] });

    // Remove @hidden attribute
    yield setHidden(browser);
    testAccessibleTree(container, { SECTION: [ { ENTRY: [ ] } ] });
  });
