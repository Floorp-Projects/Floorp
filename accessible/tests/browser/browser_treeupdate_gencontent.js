/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

addAccessibleTask(`
  <style>
    .gentext:before {
      content: "START"
    }
    .gentext:after {
      content: "END"
    }
  </style>
  <div id="container1"></div>
  <div id="container2"><div id="container2_child">text</div></div>`,
  function*(browser, accDoc) {
    const id1 = 'container1';
    const id2 = 'container2';
    let container1 = findAccessibleChildByID(accDoc, id1);
    let container2 = findAccessibleChildByID(accDoc, id2);

    let tree = {
      SECTION: [ ] // container
    };
    testAccessibleTree(container1, tree);

    tree = {
      SECTION: [ { // container2
        SECTION: [ { // container2 child
          TEXT_LEAF: [ ] // primary text
        } ]
      } ]
    };
    testAccessibleTree(container2, tree);

    let onReorder = waitForEvent(EVENT_REORDER, id1);
    // Create and add an element with CSS generated content to container1
    yield ContentTask.spawn(browser, id1, id => {
      let node = content.document.createElement('div');
      node.textContent = 'text';
      node.setAttribute('class', 'gentext');
      content.document.getElementById(id).appendChild(node);
    });
    yield onReorder;

    tree = {
      SECTION: [ // container
        { SECTION: [ // inserted node
          { STATICTEXT: [] }, // :before
          { TEXT_LEAF: [] }, // primary text
          { STATICTEXT: [] } // :after
        ] }
      ]
    };
    testAccessibleTree(container1, tree);

    onReorder = waitForEvent(EVENT_REORDER, id2);
    // Add CSS generated content to an element in container2's subtree
    yield invokeSetAttribute(browser, 'container2_child', 'class', 'gentext');
    yield onReorder;

    tree = {
      SECTION: [ // container2
        { SECTION: [ // container2 child
          { STATICTEXT: [] }, // :before
          { TEXT_LEAF: [] }, // primary text
          { STATICTEXT: [] } // :after
        ] }
      ]
    };
    testAccessibleTree(container2, tree);
  });
