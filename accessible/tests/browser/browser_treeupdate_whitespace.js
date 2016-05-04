/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

addAccessibleTask('doc_treeupdate_whitespace.html', function*(browser, accDoc) {
  let container1 = findAccessibleChildByID(accDoc, 'container1');
  let container2Parent = findAccessibleChildByID(accDoc, 'container2-parent');

  let tree = {
    SECTION: [
      { GRAPHIC: [] },
      { TEXT_LEAF: [] },
      { GRAPHIC: [] },
      { TEXT_LEAF: [] },
      { GRAPHIC: [] }
    ]
  };
  testAccessibleTree(container1, tree);

  let onReorder = waitForEvent(EVENT_REORDER, 'container1');
  // Remove img1 from container1
  yield ContentTask.spawn(browser, {}, () => {
    let doc = content.document;
    doc.getElementById('container1').removeChild(
      doc.getElementById('img1'));
  });
  yield onReorder;

  tree = {
    SECTION: [
      { GRAPHIC: [] },
      { TEXT_LEAF: [] },
      { GRAPHIC: [] }
    ]
  };
  testAccessibleTree(container1, tree);

  tree = {
    SECTION: [
      { LINK: [] },
      { LINK: [ { GRAPHIC: [] } ] }
    ]
  };
  testAccessibleTree(container2Parent, tree);

  onReorder = waitForEvent(EVENT_REORDER, 'container2-parent');
  // Append an img with valid src to container2
  yield ContentTask.spawn(browser, {}, () => {
    let doc = content.document;
    let img = doc.createElement('img');
    img.setAttribute('src',
      'http://example.com/a11y/accessible/tests/mochitest/moz.png');
    doc.getElementById('container2').appendChild(img);
  });
  yield onReorder;

  tree = {
    SECTION: [
      { LINK: [ { GRAPHIC: [ ] } ] },
      { TEXT_LEAF: [ ] },
      { LINK: [ { GRAPHIC: [ ] } ] }
    ]
  };
  testAccessibleTree(container2Parent, tree);
});
