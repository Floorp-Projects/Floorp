/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

addAccessibleTask('<select id="select"></select>', function*(browser, accDoc) {
  let select = findAccessibleChildByID(accDoc, 'select');

  let onEvent = waitForEvent(EVENT_REORDER, 'select');
  // Create a combobox with grouping and 2 standalone options
  yield ContentTask.spawn(browser, {}, () => {
    let doc = content.document;
    let select = doc.getElementById('select');
    let optGroup = doc.createElement('optgroup');

    for (let i = 0; i < 2; i++) {
      let opt = doc.createElement('option');
      opt.value = i;
      opt.text = 'Option: Value ' + i;
      optGroup.appendChild(opt);
    }
    select.add(optGroup, null);

    for (let i = 0; i < 2; i++) {
      let opt = doc.createElement('option');
      select.add(opt, null);
    }
    select.firstChild.firstChild.id = 'option1Node';
  });
  let event = yield onEvent;
  let option1Node = findAccessibleChildByID(event.accessible, 'option1Node');

  let tree = {
    COMBOBOX: [ {
      COMBOBOX_LIST: [ {
        GROUPING: [
          { COMBOBOX_OPTION: [ { TEXT_LEAF: [] } ] },
          { COMBOBOX_OPTION: [ { TEXT_LEAF: [] } ] }
        ]
      }, {
        COMBOBOX_OPTION: []
      }, {
        COMBOBOX_OPTION: []
      } ]
    } ]
  };
  testAccessibleTree(select, tree);
  ok(!isDefunct(option1Node), 'option shouldn\'t be defunct');

  onEvent = waitForEvent(EVENT_REORDER, 'select');
  // Remove grouping from combobox
  yield ContentTask.spawn(browser, {}, () => {
    let select = content.document.getElementById('select');
    select.removeChild(select.firstChild);
  });
  yield onEvent;

  tree = {
    COMBOBOX: [ {
      COMBOBOX_LIST: [
        { COMBOBOX_OPTION: [] },
        { COMBOBOX_OPTION: [] }
      ]
    } ]
  };
  testAccessibleTree(select, tree);
  ok(isDefunct(option1Node),
    'removed option shouldn\'t be accessible anymore!');

  onEvent = waitForEvent(EVENT_REORDER, 'select');
  // Remove all options from combobox
  yield ContentTask.spawn(browser, {}, () => {
    let select = content.document.getElementById('select');
    while (select.length) {
      select.remove(0);
    }
  });
  yield onEvent;

  tree = {
    COMBOBOX: [ {
      COMBOBOX_LIST: [ ]
    } ]
  };
  testAccessibleTree(select, tree);
});
