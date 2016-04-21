/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

function* testTreeOnHide(browser, accDoc, containerID, id, before, after) {
  let acc = findAccessibleChildByID(accDoc, containerID);
  testAccessibleTree(acc, before);

  let onReorder = waitForEvent(EVENT_REORDER, containerID);
  yield invokeSetStyle(browser, id, 'visibility', 'hidden');
  yield onReorder;

  testAccessibleTree(acc, after);
}

function* test3(browser, accessible) {
  let tree = {
    SECTION: [ // container
      { SECTION: [ // parent
        { SECTION: [ // child
          { TEXT_LEAF: [] }
        ] }
      ] },
      { SECTION: [ // parent2
        { SECTION: [ // child2
          { TEXT_LEAF: [] }
        ] }
      ] }
    ] };
  testAccessibleTree(accessible, tree);

  let onReorder = waitForEvent(EVENT_REORDER, 't3_container');
  yield ContentTask.spawn(browser, {}, () => {
    let doc = content.document;
    doc.getElementById('t3_container').style.color = 'red';
    doc.getElementById('t3_parent').style.visibility = 'hidden';
    doc.getElementById('t3_parent2').style.visibility = 'hidden';
  });
  yield onReorder;

  tree = {
    SECTION: [ // container
      { SECTION: [ // child
        { TEXT_LEAF: [] }
      ] },
      { SECTION: [ // child2
        { TEXT_LEAF: [] }
      ] }
    ] };
  testAccessibleTree(accessible, tree);
}

function* test4(browser, accessible) {
  let tree = {
    SECTION: [
      { TABLE: [
        { ROW: [
          { CELL: [ ] }
        ] }
      ] }
    ] };
  testAccessibleTree(accessible, tree);

  let onReorder = waitForEvent(EVENT_REORDER, 't4_parent');
  yield ContentTask.spawn(browser, {}, () => {
    let doc = content.document;
    doc.getElementById('t4_container').style.color = 'red';
    doc.getElementById('t4_child').style.visibility = 'visible';
  });
  yield onReorder;

  tree = {
    SECTION: [{
      TABLE: [{
        ROW: [{
          CELL: [{
            SECTION: [{
              TEXT_LEAF: []
            }]
          }]
        }]
      }]
    }]
  };
  testAccessibleTree(accessible, tree);
}

addAccessibleTask('doc_treeupdate_visibility.html', function*(browser, accDoc) {
  let t3Container = findAccessibleChildByID(accDoc, 't3_container');
  let t4Container = findAccessibleChildByID(accDoc, 't4_container');

  yield testTreeOnHide(browser, accDoc, 't1_container', 't1_parent', {
    SECTION: [{
      SECTION: [{
        SECTION: [ { TEXT_LEAF: [] } ]
      }]
    }]
  }, {
    SECTION: [ {
      SECTION: [ { TEXT_LEAF: [] } ]
    } ]
  });

  yield testTreeOnHide(browser, accDoc, 't2_container', 't2_grandparent', {
    SECTION: [{ // container
      SECTION: [{ // grand parent
        SECTION: [{
          SECTION: [{ // child
            TEXT_LEAF: []
          }]
        }, {
          SECTION: [{ // child2
            TEXT_LEAF: []
          }]
        }]
      }]
    }]
  }, {
    SECTION: [{ // container
      SECTION: [{ // child
        TEXT_LEAF: []
      }]
    }, {
      SECTION: [{ // child2
        TEXT_LEAF: []
      }]
    }]
  });

  yield test3(browser, t3Container);
  yield test4(browser, t4Container);

  yield testTreeOnHide(browser, accDoc, 't5_container', 't5_subcontainer', {
    SECTION: [{ // container
      SECTION: [{ // subcontainer
        TABLE: [{
          ROW: [{
            CELL: [{
              SECTION: [{ // child
                TEXT_LEAF: []
              }]
            }]
          }]
        }]
      }]
    }]
  }, {
    SECTION: [{ // container
      SECTION: [{ // child
        TEXT_LEAF: []
      }]
    }]
  });

  yield testTreeOnHide(browser, accDoc, 't6_container', 't6_subcontainer', {
    SECTION: [{ // container
      SECTION: [{ // subcontainer
        TABLE: [{
          ROW: [{
            CELL: [{
              TABLE: [{ // nested table
                ROW: [{
                  CELL: [{
                    SECTION: [{ // child
                      TEXT_LEAF: []
                    }]
                  }]
                }]
              }]
            }]
          }]
        }]
      }, {
        SECTION: [{ // child2
          TEXT_LEAF: []
        }]
      }]
    }]
  }, {
    SECTION: [{ // container
      SECTION: [{ // child
        TEXT_LEAF: []
      }]
    }, {
      SECTION: [{ // child2
        TEXT_LEAF: []
      }]
    }]
  });
});
