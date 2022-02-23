/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gClickEvents = ["mousedown", "mouseup", "click"];

const gActionDescrMap = {
  jump: "Jump",
  press: "Press",
  check: "Check",
  uncheck: "Uncheck",
  select: "Select",
  open: "Open",
  close: "Close",
  switch: "Switch",
  click: "Click",
  collapse: "Collapse",
  expand: "Expand",
  activate: "Activate",
  cycle: "Cycle",
};

const isCacheEnabled = Services.prefs.getBoolPref(
  "accessibility.cache.enabled",
  false
);
// Some RemoteAccessible methods aren't supported on Windows when the cache is
// disabled.
const isWinNoCache = !isCacheEnabled && AppConstants.platform == "win";

async function testActions(browser, docAcc, id, expectedActions, domEvents) {
  const acc = findAccessibleChildByID(docAcc, id);
  is(acc.actionCount, expectedActions.length, "Correct action count");

  let actionNames = [];
  let actionDescriptions = [];
  for (let i = 0; i < acc.actionCount; i++) {
    actionNames.push(acc.getActionName(i));
    actionDescriptions.push(acc.getActionDescription(i));
  }

  is(actionNames.join(","), expectedActions.join(","), "Correct action names");
  is(
    actionDescriptions.join(","),
    expectedActions.map(a => gActionDescrMap[a]).join(","),
    "Correct action descriptions"
  );

  if (!domEvents) {
    return;
  }

  // We need to set up the listener, and wait for the promise in two separate
  // content tasks.
  await invokeContentTask(browser, [id, domEvents], (_id, _domEvents) => {
    let promises = _domEvents.map(
      evtName =>
        new Promise(resolve => {
          const listener = e => {
            if (e.target.id == _id) {
              content.removeEventListener(evtName, listener);
              content.evtPromise = null;
              resolve(42);
            }
          };
          content.addEventListener(evtName, listener);
        })
    );
    content.evtPromise = Promise.all(promises);
  });

  acc.doAction(0);

  let eventFired = await invokeContentTask(browser, [], async () => {
    await content.evtPromise;
    return true;
  });

  ok(eventFired, `DOM events fired '${domEvents}'`);
}

addAccessibleTask(
  `<ul>
    <li id="li_clickable1" onclick="">Clickable list item</li>
    <li id="li_clickable2" onmousedown="">Clickable list item</li>
    <li id="li_clickable3" onmouseup="">Clickable list item</li>
  </ul>

  <img id="onclick_img" onclick=""
        src="http://example.com/a11y/accessible/tests/mochitest/moz.png">

  <a id="link1" href="#">linkable textleaf accessible</a>
  <div id="link2" onclick="">linkable textleaf accessible</div>

  <div>
    <label for="TextBox_t2" id="label1">
      <span>Explicit</span>
    </label>
    <input name="in2" id="TextBox_t2" type="text" maxlength="17">
  </div>
  `,
  async function(browser, docAcc) {
    const _testActions = async (id, expectedActions, domEvents) => {
      await testActions(browser, docAcc, id, expectedActions, domEvents);
    };

    await _testActions("li_clickable1", ["click"], gClickEvents);
    await _testActions("li_clickable2", ["click"], gClickEvents);
    await _testActions("li_clickable3", ["click"], gClickEvents);

    await _testActions("onclick_img", ["click"], gClickEvents);
    await _testActions("link1", ["jump"], gClickEvents);
    await _testActions("link2", ["click"], gClickEvents);
    await _testActions("label1", ["click"], gClickEvents);

    await invokeContentTask(browser, [], () => {
      content.document
        .getElementById("li_clickable1")
        .removeAttribute("onclick");
    });

    let acc = findAccessibleChildByID(docAcc, "li_clickable1");
    await untilCacheIs(() => acc.actionCount, 0, "li has no actions");
    let thrown = false;
    try {
      acc.doAction(0);
    } catch (e) {
      thrown = true;
    }
    ok(thrown, "doAction should throw exception");

    // Remove 'for' from label
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("label1").removeAttribute("for");
    });
    acc = findAccessibleChildByID(docAcc, "label1");
    await untilCacheIs(() => acc.actionCount, 0, "label has no actions");
    thrown = false;
    try {
      acc.doAction(0);
      ok(false, "doAction should throw exception");
    } catch (e) {
      thrown = true;
    }
    ok(thrown, "doAction should throw exception");

    // Add 'longdesc' to image
    await invokeContentTask(browser, [], () => {
      content.document
        .getElementById("onclick_img")
        .setAttribute("longdesc", "http://example.com");
    });
    acc = findAccessibleChildByID(docAcc, "onclick_img");
    await untilCacheIs(() => acc.actionCount, 2, "img has 2 actions");
    await _testActions("onclick_img", ["click", "showlongdesc"]);

    // Remove 'onclick' from image with 'longdesc'
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("onclick_img").removeAttribute("onclick");
    });
    acc = findAccessibleChildByID(docAcc, "onclick_img");
    await untilCacheIs(() => acc.actionCount, 1, "img has 1 actions");
    await _testActions("onclick_img", ["showlongdesc"]);
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
    remoteIframe: !isWinNoCache,
  }
);
