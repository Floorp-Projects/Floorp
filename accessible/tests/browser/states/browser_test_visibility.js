/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR },
            { name: 'states.js', dir: MOCHITESTS_DIR });

async function runTest(browser, accDoc) {
  let getAcc = id => findAccessibleChildByID(accDoc, id);

  testStates(getAcc("div"), 0, 0, STATE_INVISIBLE | STATE_OFFSCREEN);

  let input = getAcc("input_scrolledoff");
  testStates(input, STATE_OFFSCREEN, 0, STATE_INVISIBLE);

  // scrolled off item (twice)
  let lastLi = getAcc("li_last");
  testStates(lastLi, STATE_OFFSCREEN, 0, STATE_INVISIBLE);

  // scroll into view the item
  await ContentTask.spawn(browser, {}, () => {
    content.document.getElementById('li_last').scrollIntoView(true);
  });
  testStates(lastLi, 0, 0, STATE_OFFSCREEN | STATE_INVISIBLE);

  // first item is scrolled off now (testcase for bug 768786)
  let firstLi = getAcc("li_first");
  testStates(firstLi, STATE_OFFSCREEN, 0, STATE_INVISIBLE);

  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  // Accessibles in background tab should have offscreen state and no
  // invisible state.
  testStates(getAcc("div"), STATE_OFFSCREEN, 0, STATE_INVISIBLE);
  await BrowserTestUtils.removeTab(newTab);
}

addAccessibleTask(`
  <div id="div" style="border:2px solid blue; width: 500px; height: 110vh;"></div>
  <input id="input_scrolledoff">
  <ul style="border:2px solid red; width: 100px; height: 50px; overflow: auto;">
    <li id="li_first">item1</li><li>item2</li><li>item3</li>
    <li>item4</li><li>item5</li><li id="li_last">item6</li>
  </ul>`, runTest
);
