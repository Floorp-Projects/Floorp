/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR },
            { name: 'states.js', dir: MOCHITESTS_DIR });

async function runTests(browser, accDoc) {
  let getAcc = id => findAccessibleChildByID(accDoc, id);

  // a: no traversed state
  testStates(getAcc("link_traversed"), 0, 0, STATE_TRAVERSED);

  let onStateChanged = waitForEvent(EVENT_STATE_CHANGE, "link_traversed");
  let newWinOpened = BrowserTestUtils.waitForNewWindow();

  await BrowserTestUtils.synthesizeMouse('#link_traversed',
    1, 1, { shiftKey: true }, browser);

  await onStateChanged;
  testStates(getAcc("link_traversed"), STATE_TRAVERSED);

  let newWin = await newWinOpened;
  await BrowserTestUtils.closeWindow(newWin);
}

/**
 * Test caching of accessible object states
 */
addAccessibleTask(`
  <a id="link_traversed" href="http://www.example.com" target="_top">
    example.com
  </a>`, runTests
);
