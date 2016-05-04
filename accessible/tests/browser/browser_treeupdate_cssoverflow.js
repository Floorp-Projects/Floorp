/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

addAccessibleTask(`
  <div id="container"><div id="scrollarea" style="overflow:auto;"><input>
  </div></div>
  <div id="container2"><div id="scrollarea2" style="overflow:hidden;">
  </div></div>`, function*(browser, accDoc) {
  const id1 = 'container';
  const id2 = 'container2';
  const container = findAccessibleChildByID(accDoc, id1);
  const container2 = findAccessibleChildByID(accDoc, id2);

  /* ================= Change scroll range ================================== */
  let tree = {
    SECTION: [ {// container
      SECTION: [ {// scroll area
        ENTRY: [ ] // child content
      } ]
    } ]
  };
  testAccessibleTree(container, tree);

  let onReorder = waitForEvent(EVENT_REORDER, id1);
  yield ContentTask.spawn(browser, id1, id => {
    let doc = content.document;
    doc.getElementById('scrollarea').style.width = '20px';
    doc.getElementById(id).appendChild(doc.createElement('input'));
  });
  yield onReorder;

  tree = {
    SECTION: [ {// container
      SECTION: [ {// scroll area
        ENTRY: [ ] // child content
      } ]
    }, {
      ENTRY: [ ] // inserted input
    } ]
  };
  testAccessibleTree(container, tree);

  /* ================= Change scrollbar styles ============================== */
  tree = { SECTION: [ ] };
  testAccessibleTree(container2, tree);

  onReorder = waitForEvent(EVENT_REORDER, id2);
  yield invokeSetStyle(browser, 'scrollarea2', 'overflow', 'auto');
  yield onReorder;

  tree = {
    SECTION: [ // container
      { SECTION: [] } // scroll area
    ]
  };
  testAccessibleTree(container2, tree);
});
