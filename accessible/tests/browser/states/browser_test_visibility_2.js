/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test tables, table rows are reported on screen, even if some cells of a given row are
 * offscreen.
 */
addAccessibleTask(
  `
  <table id="table" style="width:150vw;" border><tr id="row"><td id="one" style="width:50vw;">one</td><td style="width:50vw;" id="two">two</td><td id="three">three</td></tr></table>
  `,
  async function (browser, accDoc) {
    const table = findAccessibleChildByID(accDoc, "table");
    const row = findAccessibleChildByID(accDoc, "row");
    const one = findAccessibleChildByID(accDoc, "one");
    const two = findAccessibleChildByID(accDoc, "two");
    const three = findAccessibleChildByID(accDoc, "three");

    await untilCacheOk(
      () => testVisibility(table, false, false),
      "table should be on screen and visible"
    );
    await untilCacheOk(
      () => testVisibility(row, false, false),
      "row should be on screen and visible"
    );
    await untilCacheOk(
      () => testVisibility(one, false, false),
      "one should be on screen and visible"
    );
    await untilCacheOk(
      () => testVisibility(two, false, false),
      "two should be on screen and visible"
    );
    await untilCacheOk(
      () => testVisibility(three, true, false),
      "three should be off screen and visible"
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

/**
 * Test rows and cells outside of the viewport are reported as offscreen.
 */
addAccessibleTask(
  `
  <table id="table" style="height:150vh;" border><tr style="height:100vh;" id="rowA"><td id="one">one</td></tr><tr id="rowB"><td id="two">two</td></tr></table>
  `,
  async function (browser, accDoc) {
    const table = findAccessibleChildByID(accDoc, "table");
    const rowA = findAccessibleChildByID(accDoc, "rowA");
    const one = findAccessibleChildByID(accDoc, "one");
    const rowB = findAccessibleChildByID(accDoc, "rowB");
    const two = findAccessibleChildByID(accDoc, "two");

    await untilCacheOk(
      () => testVisibility(table, false, false),
      "table should be on screen and visible"
    );
    await untilCacheOk(
      () => testVisibility(rowA, false, false),
      "rowA should be on screen and visible"
    );
    await untilCacheOk(
      () => testVisibility(one, false, false),
      "one should be on screen and visible"
    );
    await untilCacheOk(
      () => testVisibility(rowB, true, false),
      "rowB should be off screen and visible"
    );
    await untilCacheOk(
      () => testVisibility(two, true, false),
      "two should be off screen and visible"
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

addAccessibleTask(
  `
  <div id="div">hello</div>
  `,
  async function (browser, accDoc) {
    let textLeaf = findAccessibleChildByID(accDoc, "div").firstChild;
    await untilCacheOk(
      () => testVisibility(textLeaf, false, false),
      "text should be on screen and visible"
    );
    let p = waitForEvent(EVENT_TEXT_INSERTED, "div");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("div").textContent = "goodbye";
    });
    await p;
    textLeaf = findAccessibleChildByID(accDoc, "div").firstChild;
    await untilCacheOk(
      () => testVisibility(textLeaf, false, false),
      "text should be on screen and visible"
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

/**
 * Overlapping, opaque divs with the same bounds should not be considered
 * offscreen.
 */
addAccessibleTask(
  `
  <style>div { height: 5px; width: 5px; background: green; }</style>
  <div id="outer" role="group"><div style="background:blue;" id="inner" role="group">hi</div></div>
  `,
  async function (browser, accDoc) {
    const outer = findAccessibleChildByID(accDoc, "outer");
    const inner = findAccessibleChildByID(accDoc, "inner");

    await untilCacheOk(
      () => testVisibility(outer, false, false),
      "outer should be on screen and visible"
    );
    await untilCacheOk(
      () => testVisibility(inner, false, false),
      "inner should be on screen and visible"
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);
