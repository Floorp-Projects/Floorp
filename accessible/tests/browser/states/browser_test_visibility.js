/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

function testVisibility(acc, shouldBeOffscreen, shouldBeInvisible) {
  const [states] = getStates(acc);
  let looksGood = shouldBeOffscreen == ((states & STATE_OFFSCREEN) != 0);
  looksGood |= shouldBeInvisible == ((states & STATE_INVISIBLE) != 0);
  return looksGood;
}

async function runTest(browser, accDoc) {
  let getAcc = id => findAccessibleChildByID(accDoc, id);

  await untilCacheOk(
    () => testVisibility(getAcc("div"), false, false),
    "Div should be on screen"
  );

  let input = getAcc("input_scrolledoff");
  await untilCacheOk(
    () => testVisibility(input, true, false),
    "Input should be offscreen"
  );

  // scrolled off item (twice)
  let lastLi = getAcc("li_last");
  await untilCacheOk(
    () => testVisibility(lastLi, true, false),
    "Last list item should be offscreen"
  );

  // scroll into view the item
  await invokeContentTask(browser, [], () => {
    content.document.getElementById("li_last").scrollIntoView(true);
  });
  await untilCacheOk(
    () => testVisibility(lastLi, false, false),
    "Last list item should no longer be offscreen"
  );

  // first item is scrolled off now (testcase for bug 768786)
  let firstLi = getAcc("li_first");
  await untilCacheOk(
    () => testVisibility(firstLi, true, false),
    "First listitem should now be offscreen"
  );

  await untilCacheOk(
    () => testVisibility(getAcc("frame"), false, false),
    "iframe should initially be onscreen"
  );

  let loaded = waitForEvent(EVENT_DOCUMENT_LOAD_COMPLETE, "iframeDoc");
  await invokeContentTask(browser, [], () => {
    content.document.querySelector("iframe").src =
      'data:text/html,<body id="iframeDoc"><p id="p">hi</p></body>';
  });

  const iframeDoc = (await loaded).accessible;
  await untilCacheOk(
    () => testVisibility(getAcc("frame"), false, false),
    "iframe outer doc should now be on screen"
  );
  await untilCacheOk(
    () => testVisibility(iframeDoc, false, false),
    "iframe inner doc should be on screen"
  );
  const iframeP = findAccessibleChildByID(iframeDoc, "p");
  await untilCacheOk(
    () => testVisibility(iframeP, false, false),
    "iframe content should also be on screen"
  );

  // scroll into view the div
  await invokeContentTask(browser, [], () => {
    content.document.getElementById("div").scrollIntoView(true);
  });

  await untilCacheOk(
    () => testVisibility(getAcc("frame"), true, false),
    "iframe outer doc should now be off screen"
  );
  await untilCacheOk(
    () => testVisibility(iframeDoc, true, false),
    "iframe inner doc should now be off screen"
  );
  await untilCacheOk(
    () => testVisibility(iframeP, true, false),
    "iframe content should now be off screen"
  );

  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  // Accessibles in background tab should have offscreen state and no
  // invisible state.
  await untilCacheOk(
    () => testVisibility(getAcc("div"), true, false),
    "Accs in background tab should be offscreen but not invisible."
  );

  await untilCacheOk(
    () => testVisibility(getAcc("frame"), true, false),
    "iframe outer doc should still be off screen"
  );
  await untilCacheOk(
    () => testVisibility(iframeDoc, true, false),
    "iframe inner doc should still be off screen"
  );
  await untilCacheOk(
    () => testVisibility(iframeP, true, false),
    "iframe content should still be off screen"
  );

  BrowserTestUtils.removeTab(newTab);
}

addAccessibleTask(
  `
  <div id="div" style="border:2px solid blue; width: 500px; height: 110vh;"></div>
  <input id="input_scrolledoff">
  <ul style="border:2px solid red; width: 100px; height: 50px; overflow: auto;">
    <li id="li_first">item1</li><li>item2</li><li>item3</li>
    <li>item4</li><li>item5</li><li id="li_last">item6</li>
  </ul>
  <iframe id="frame"></iframe>
  `,
  runTest,
  { chrome: true, iframe: true, remoteIframe: true }
);
