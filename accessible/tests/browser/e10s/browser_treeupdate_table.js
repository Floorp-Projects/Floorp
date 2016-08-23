/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

addAccessibleTask(`
  <table id="table">
    <tr>
      <td>cell1</td>
      <td>cell2</td>
    </tr>
  </table>`, function*(browser, accDoc) {
  let table = findAccessibleChildByID(accDoc, 'table');

  let tree = {
    TABLE: [
      { ROW: [
        { CELL: [ {TEXT_LEAF: [] }]},
        { CELL: [ {TEXT_LEAF: [] }]}
      ] }
    ]
  };
  testAccessibleTree(table, tree);

  let onReorder = waitForEvent(EVENT_REORDER, 'table');
  yield ContentTask.spawn(browser, {}, () => {
    // append a caption, it should appear as a first element in the
    // accessible tree.
    let doc = content.document;
    let caption = doc.createElement('caption');
    caption.textContent = 'table caption';
    doc.getElementById('table').appendChild(caption);
  });
  yield onReorder;

  tree = {
    TABLE: [
      { CAPTION: [ { TEXT_LEAF: [] } ] },
      { ROW: [
        { CELL: [ {TEXT_LEAF: [] }]},
        { CELL: [ {TEXT_LEAF: [] }]}
      ] }
    ]
  };
  testAccessibleTree(table, tree);
});
