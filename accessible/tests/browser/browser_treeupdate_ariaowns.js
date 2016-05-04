/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

function* testContainer1(browser, accDoc) {
  const id = 't1_container';
  const docID = getAccessibleDOMNodeID(accDoc);
  const acc = findAccessibleChildByID(accDoc, id);

  /* ================= Initial tree test ==================================== */
  // children are swapped by ARIA owns
  let tree = {
    SECTION: [
      { CHECKBUTTON: [
        { SECTION: [] }
      ] },
      { PUSHBUTTON: [ ] }
    ]
  };
  testAccessibleTree(acc, tree);

  /* ================ Change ARIA owns ====================================== */
  let onReorder = waitForEvent(EVENT_REORDER, id);
  yield invokeSetAttribute(browser, id, 'aria-owns', 't1_button t1_subdiv');
  yield onReorder;

  // children are swapped again, button and subdiv are appended to
  // the children.
  tree = {
    SECTION: [
      { CHECKBUTTON: [ ] }, // checkbox, native order
      { PUSHBUTTON: [ ] }, // button, rearranged by ARIA own
      { SECTION: [ ] } // subdiv from the subtree, ARIA owned
    ]
  };
  testAccessibleTree(acc, tree);

  /* ================ Remove ARIA owns ====================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  yield invokeSetAttribute(browser, id, 'aria-owns');
  yield onReorder;

  // children follow the DOM order
  tree = {
    SECTION: [
      { PUSHBUTTON: [ ] },
      { CHECKBUTTON: [
          { SECTION: [] }
      ] }
    ]
  };
  testAccessibleTree(acc, tree);

  /* ================ Set ARIA owns ========================================= */
  onReorder = waitForEvent(EVENT_REORDER, id);
  yield invokeSetAttribute(browser, id, 'aria-owns', 't1_button t1_subdiv');
  yield onReorder;

  // children are swapped again, button and subdiv are appended to
  // the children.
  tree = {
    SECTION: [
      { CHECKBUTTON: [ ] }, // checkbox
      { PUSHBUTTON: [ ] }, // button, rearranged by ARIA own
      { SECTION: [ ] } // subdiv from the subtree, ARIA owned
    ]
  };
  testAccessibleTree(acc, tree);

  /* ================ Add ID to ARIA owns =================================== */
  onReorder = waitForEvent(EVENT_REORDER, docID);
  yield invokeSetAttribute(browser, id, 'aria-owns',
    't1_button t1_subdiv t1_group');
  yield onReorder;

  // children are swapped again, button and subdiv are appended to
  // the children.
  tree = {
    SECTION: [
      { CHECKBUTTON: [ ] }, // t1_checkbox
      { PUSHBUTTON: [ ] }, // button, t1_button
      { SECTION: [ ] }, // subdiv from the subtree, t1_subdiv
      { GROUPING: [ ] } // group from outside, t1_group
    ]
  };
  testAccessibleTree(acc, tree);

  /* ================ Append element ======================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  yield ContentTask.spawn(browser, id, id => {
    let div = content.document.createElement('div');
    div.setAttribute('id', 't1_child3');
    div.setAttribute('role', 'radio');
    content.document.getElementById(id).appendChild(div);
  });
  yield onReorder;

  // children are invalidated, they includes aria-owns swapped kids and
  // newly inserted child.
  tree = {
    SECTION: [
      { CHECKBUTTON: [ ] }, // existing explicit, t1_checkbox
      { RADIOBUTTON: [ ] }, // new explicit, t1_child3
      { PUSHBUTTON: [ ] }, // ARIA owned, t1_button
      { SECTION: [ ] }, // ARIA owned, t1_subdiv
      { GROUPING: [ ] } // ARIA owned, t1_group
    ]
  };
  testAccessibleTree(acc, tree);

  /* ================ Remove element ======================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  yield ContentTask.spawn(browser, {}, () =>
    content.document.getElementById('t1_span').parentNode.removeChild(
      content.document.getElementById('t1_span')));
  yield onReorder;

  // subdiv should go away
  tree = {
    SECTION: [
      { CHECKBUTTON: [ ] }, // explicit, t1_checkbox
      { RADIOBUTTON: [ ] }, // explicit, t1_child3
      { PUSHBUTTON: [ ] }, // ARIA owned, t1_button
      { GROUPING: [ ] } // ARIA owned, t1_group
    ]
  };
  testAccessibleTree(acc, tree);

  /* ================ Remove ID ============================================= */
  onReorder = waitForEvent(EVENT_REORDER, docID);
  yield invokeSetAttribute(browser, 't1_group', 'id');
  yield onReorder;

  tree = {
    SECTION: [
      { CHECKBUTTON: [ ] },
      { RADIOBUTTON: [ ] },
      { PUSHBUTTON: [ ] } // ARIA owned, t1_button
    ]
  };
  testAccessibleTree(acc, tree);

  /* ================ Set ID ================================================ */
  onReorder = waitForEvent(EVENT_REORDER, docID);
  yield invokeSetAttribute(browser, 't1_grouptmp', 'id', 't1_group');
  yield onReorder;

  tree = {
    SECTION: [
      { CHECKBUTTON: [ ] },
      { RADIOBUTTON: [ ] },
      { PUSHBUTTON: [ ] }, // ARIA owned, t1_button
      { GROUPING: [ ] } // ARIA owned, t1_group, previously t1_grouptmp
    ]
  };
  testAccessibleTree(acc, tree);
}

function* removeContainer(browser, accDoc) {
  const id = 't2_container1';
  const acc = findAccessibleChildByID(accDoc, id);

  let tree = {
    SECTION: [
      { CHECKBUTTON: [ ] } // ARIA owned, 't2_owned'
    ]
  };
  testAccessibleTree(acc, tree);

  let onReorder = waitForEvent(EVENT_REORDER, id);
  yield ContentTask.spawn(browser, {}, () =>
    content.document.getElementById('t2_container2').removeChild(
      content.document.getElementById('t2_container3')));
  yield onReorder;

  tree = {
    SECTION: [ ]
  };
  testAccessibleTree(acc, tree);
}

function* stealAndRecacheChildren(browser, accDoc) {
  const id1 = 't3_container1';
  const id2 = 't3_container2';
  const acc1 = findAccessibleChildByID(accDoc, id1);
  const acc2 = findAccessibleChildByID(accDoc, id2);

  /* ================ Steal from other ARIA owns ============================ */
  let onReorder = waitForEvent(EVENT_REORDER, id2);
  yield invokeSetAttribute(browser, id2, 'aria-owns', 't3_child');
  yield onReorder;

  let tree = {
    SECTION: [ ]
  };
  testAccessibleTree(acc1, tree);

  tree = {
    SECTION: [
      { CHECKBUTTON: [ ] }
    ]
  };
  testAccessibleTree(acc2, tree);

  /* ================ Append element to recache children ==================== */
  onReorder = waitForEvent(EVENT_REORDER, id2);
  yield ContentTask.spawn(browser, id2, id => {
    let div = content.document.createElement('div');
    div.setAttribute('role', 'radio');
    content.document.getElementById(id).appendChild(div);
  });
  yield onReorder;

  tree = {
    SECTION: [ ]
  };
  testAccessibleTree(acc1, tree);

  tree = {
    SECTION: [
      { RADIOBUTTON: [ ] },
      { CHECKBUTTON: [ ] } // ARIA owned
    ]
  };
  testAccessibleTree(acc2, tree);
}

function* showHiddenElement(browser, accDoc) {
  const id = 't4_container1';
  const acc = findAccessibleChildByID(accDoc, id);

  let tree = {
    SECTION: [
      { RADIOBUTTON: [] }
    ]
  };
  testAccessibleTree(acc, tree);

  let onReorder = waitForEvent(EVENT_REORDER, id);
  yield invokeSetStyle(browser, 't4_child1', 'display', 'block');
  yield onReorder;

  tree = {
    SECTION: [
      { CHECKBUTTON: [] },
      { RADIOBUTTON: [] }
    ]
  };
  testAccessibleTree(acc, tree);
}

function* rearrangeARIAOwns(browser, accDoc) {
  const id = 't5_container';
  const acc = findAccessibleChildByID(accDoc, id);
  const tests = [{
    val: 't5_checkbox t5_radio t5_button',
    roleList: [ 'CHECKBUTTON', 'RADIOBUTTON', 'PUSHBUTTON' ]
  }, {
    val: 't5_radio t5_button t5_checkbox',
    roleList: [ 'RADIOBUTTON', 'PUSHBUTTON', 'CHECKBUTTON' ]
  }];

  for (let { val, roleList } of tests) {
    let onReorder = waitForEvent(EVENT_REORDER, id);
    yield invokeSetAttribute(browser, id, 'aria-owns', val);
    yield onReorder;

    let tree = { SECTION: [ ] };
    for (let role of roleList) {
      let ch = {};
      ch[role] = [];
      tree.SECTION.push(ch);
    }
    testAccessibleTree(acc, tree);
  }
}

function* removeNotARIAOwnedEl(browser, accDoc) {
  const id = 't6_container';
  const acc = findAccessibleChildByID(accDoc, id);

  let tree = {
    SECTION: [
      { TEXT_LEAF: [ ] },
      { GROUPING: [ ] }
    ]
  };
  testAccessibleTree(acc, tree);

  let onReorder = waitForEvent(EVENT_REORDER, id);
  yield ContentTask.spawn(browser, id, id =>
    content.document.getElementById(id).removeChild(
      content.document.getElementById('t6_span')));
  yield onReorder;

  tree = {
    SECTION: [
      { GROUPING: [ ] }
    ]
  };
  testAccessibleTree(acc, tree);
}

addAccessibleTask('doc_treeupdate_ariaowns.html', function*(browser, accDoc) {
  yield testContainer1(browser, accDoc);
  yield removeContainer(browser, accDoc);
  yield stealAndRecacheChildren(browser, accDoc);
  yield showHiddenElement(browser, accDoc);
  yield rearrangeARIAOwns(browser, accDoc);
  yield removeNotARIAOwnedEl(browser, accDoc);
});
