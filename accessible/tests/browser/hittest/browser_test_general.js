/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function runTests(browser, accDoc) {
  const dpr = await getContentDPR(browser);

  await testChildAtPoint(
    dpr,
    3,
    3,
    findAccessibleChildByID(accDoc, "list"),
    findAccessibleChildByID(accDoc, "listitem"),
    findAccessibleChildByID(accDoc, "inner").firstChild
  );
  todo(
    false,
    "Bug 746974 - children must match on all platforms. On Windows, " +
      "ChildAtPoint with eDeepestChild is incorrectly ignoring MustPrune " +
      "for the graphic."
  );

  const txt = findAccessibleChildByID(accDoc, "txt");
  await testChildAtPoint(dpr, 1, 1, txt, txt, txt);

  info(
    "::MustPrune case, point is outside of textbox accessible but is in document."
  );
  await testChildAtPoint(dpr, -1, -1, txt, null, null);

  info("::MustPrune case, point is outside of root accessible.");
  await testChildAtPoint(dpr, -10000, -10000, txt, null, null);

  info("Not specific case, point is inside of btn accessible.");
  const btn = findAccessibleChildByID(accDoc, "btn");
  await testChildAtPoint(dpr, 1, 1, btn, btn, btn);

  info("Not specific case, point is outside of btn accessible.");
  await testChildAtPoint(dpr, -1, -1, btn, null, null);

  info(
    "Out of flow accessible testing, do not return out of flow accessible " +
      "because it's not a child of the accessible even though visually it is."
  );
  await invokeContentTask(browser, [], () => {
    const { CommonUtils } = ChromeUtils.importESModule(
      "chrome://mochitests/content/browser/accessible/tests/browser/Common.sys.mjs"
    );
    const doc = content.document;
    const rectArea = CommonUtils.getNode("area", doc).getBoundingClientRect();
    const outOfFlow = CommonUtils.getNode("outofflow", doc);
    outOfFlow.style.left = rectArea.left + "px";
    outOfFlow.style.top = rectArea.top + "px";
  });

  const area = findAccessibleChildByID(accDoc, "area");
  await testChildAtPoint(dpr, 1, 1, area, area, area);

  info("Test image maps. Their children are not in the layout tree.");
  await waitForImageMap(browser, accDoc);
  const imgmap = findAccessibleChildByID(accDoc, "imgmap");
  ok(imgmap, "Image map exists");
  const theLetterA = imgmap.firstChild;
  await hitTest(browser, imgmap, theLetterA, theLetterA);
  await hitTest(
    browser,
    findAccessibleChildByID(accDoc, "container"),
    imgmap,
    theLetterA
  );

  info("hit testing for element contained by zero-width element");
  const container2Input = findAccessibleChildByID(accDoc, "container2_input");
  await hitTest(
    browser,
    findAccessibleChildByID(accDoc, "container2"),
    container2Input,
    container2Input
  );

  info("hittesting table, row, cells -- rows are not in the layout tree");
  const table = findAccessibleChildByID(accDoc, "table");
  const row = findAccessibleChildByID(accDoc, "row");
  const cell1 = findAccessibleChildByID(accDoc, "cell1");

  await hitTest(browser, table, row, cell1);

  info("Testing that an inaccessible child doesn't break hit testing");
  const containerWithInaccessibleChild = findAccessibleChildByID(
    accDoc,
    "containerWithInaccessibleChild"
  );
  const containerWithInaccessibleChildP2 = findAccessibleChildByID(
    accDoc,
    "containerWithInaccessibleChild_p2"
  );
  await hitTest(
    browser,
    containerWithInaccessibleChild,
    containerWithInaccessibleChildP2,
    containerWithInaccessibleChildP2.firstChild
  );

  info("Testing wrapped text");
  const wrappedTextLinkFirstP = findAccessibleChildByID(
    accDoc,
    "wrappedTextLinkFirstP"
  );
  const wrappedTextLinkFirstA = findAccessibleChildByID(
    accDoc,
    "wrappedTextLinkFirstA"
  );
  await hitTest(
    browser,
    wrappedTextLinkFirstP,
    wrappedTextLinkFirstA,
    wrappedTextLinkFirstA.firstChild
  );
  const wrappedTextLeafFirstP = findAccessibleChildByID(
    accDoc,
    "wrappedTextLeafFirstP"
  );
  const wrappedTextLeafFirstMark = findAccessibleChildByID(
    accDoc,
    "wrappedTextLeafFirstMark"
  );
  await hitTest(
    browser,
    wrappedTextLeafFirstP,
    wrappedTextLeafFirstMark,
    wrappedTextLeafFirstMark.firstChild
  );

  info("Testing image");
  const imageP = findAccessibleChildByID(accDoc, "imageP");
  const image = findAccessibleChildByID(accDoc, "image");
  await hitTest(browser, imageP, image, image);

  info("Testing image map with 0-sized area");
  const mapWith0AreaP = findAccessibleChildByID(accDoc, "mapWith0AreaP");
  const mapWith0Area = findAccessibleChildByID(accDoc, "mapWith0Area");
  await hitTest(browser, mapWith0AreaP, mapWith0Area, mapWith0Area);
}

addAccessibleTask(
  `
  <div role="list" id="list">
    <div role="listitem" id="listitem"><span title="foo" id="inner">inner</span>item</div>
  </div>

  <span role="button">button1</span><span role="button" id="btn">button2</span>

  <span role="textbox">textbox1</span><span role="textbox" id="txt">textbox2</span>

  <div id="outofflow" style="width: 10px; height: 10px; position: absolute; left: 0px; top: 0px; background-color: yellow;">
  </div>
  <div id="area" style="width: 100px; height: 100px; background-color: blue;"></div>

  <map name="atoz_map">
    <area id="thelettera" href="http://www.bbc.co.uk/radio4/atoz/index.shtml#a"
          coords="0,0,15,15" alt="thelettera" shape="rect"/>
  </map>

  <div id="container">
    <img id="imgmap" width="447" height="15" usemap="#atoz_map" src="http://example.com/a11y/accessible/tests/mochitest/letters.gif"/>
  </div>

  <div id="container2" style="width: 0px">
    <input id="container2_input">
  </div>

  <table id="table" border>
    <tr id="row">
      <td id="cell1">hello</td>
      <td id="cell2">world</td>
    </tr>
  </table>

  <div id="containerWithInaccessibleChild">
    <p>hi</p>
    <p aria-hidden="true">hi</p>
    <p id="containerWithInaccessibleChild_p2">bye</p>
  </div>

  <p id="wrappedTextLinkFirstP" style="width: 3ch; font-family: monospace;">
    <a id="wrappedTextLinkFirstA" href="https://example.com/">a</a>b cd
  </p>

  <p id="wrappedTextLeafFirstP" style="width: 3ch; font-family: monospace;">
    <mark id="wrappedTextLeafFirstMark">a</mark><a href="https://example.com/">b cd</a>
  </p>

  <p id="imageP">
    <img id="image" src="http://example.com/a11y/accessible/tests/mochitest/letters.gif">
  </p>

  <map id="0Area">
    <area shape="rect">
  </map>
  <p id="mapWith0AreaP">
    <img id="mapWith0Area" src="http://example.com/a11y/accessible/tests/mochitest/letters.gif" usemap="#0Area">
  </p>
  `,
  runTests,
  {
    iframe: true,
    remoteIframe: true,
    // Ensure that all hittest elements are in view.
    iframeAttrs: { style: "width: 600px; height: 600px; padding: 10px;" },
  }
);

addAccessibleTask(
  `
  <div id="container">
    <h1 id="a">A</h1><h1 id="b">B</h1>
  </div>
  `,
  async function (browser, accDoc) {
    const a = findAccessibleChildByID(accDoc, "a");
    const b = findAccessibleChildByID(accDoc, "b");
    const dpr = await getContentDPR(browser);
    // eslint-disable-next-line no-unused-vars
    const [x, y, w, h] = Layout.getBounds(a, dpr);
    // The point passed below will be made relative to `b`, but
    // we'd like to test a point within `a`. Pass `a`s negative
    // width for an x offset. Pass zero as a y offset,
    // assuming the headings are on the same line.
    await testChildAtPoint(dpr, -w, 0, b, null, null);
  },
  {
    iframe: true,
    remoteIframe: true,
    // Ensure that all hittest elements are in view.
    iframeAttrs: { style: "width: 600px; height: 600px; padding: 10px;" },
  }
);

addAccessibleTask(
  `
  <style>
    div {
      width: 50px;
      height: 50px;
      position: relative;
    }

    div > div {
      width: 30px;
      height: 30px;
      position: absolute;
      opacity: 0.9;
    }
  </style>
  <div id="a" style="background-color: orange;">
    <div id="aa" style="background-color: purple;"></div>
  </div>
  <div id="b" style="background-color: yellowgreen;">
    <div id="bb" style="top: -30px; background-color: turquoise"></div>
  </div>`,
  async function (browser, accDoc) {
    const a = findAccessibleChildByID(accDoc, "a");
    const aa = findAccessibleChildByID(accDoc, "aa");
    const dpr = await getContentDPR(browser);
    const [, , w, h] = Layout.getBounds(a, dpr);
    // test upper left of `a`
    await testChildAtPoint(dpr, 1, 1, a, aa, aa);
    // test upper right of `a`
    await testChildAtPoint(dpr, w - 1, 1, a, a, a);
    // test just outside upper left of `a`
    await testChildAtPoint(dpr, 1, -1, a, null, null);
    // test halfway down/left of `a`
    await testChildAtPoint(dpr, 1, Math.round(h / 2), a, a, a);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: false,
    remoteIframe: false,
    // Ensure that all hittest elements are in view.
    iframeAttrs: { style: "width: 600px; height: 600px; padding: 10px;" },
  }
);

/**
 * Verify that hit testing returns the proper accessible when one acc content
 * is partially hidden due to overflow:hidden;
 */
addAccessibleTask(
  `
  <style>
    div div {
      overflow: hidden;
      font-family: monospace;
      width: 2ch;
    }
  </style>
  <div id="container" style="display: flex; flex-direction: row-reverse;">
    <div id="aNode">abcde</div><div id="fNode">fghij</div>
  </div>`,
  async function (browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "container");
    const aNode = findAccessibleChildByID(docAcc, "aNode");
    const fNode = findAccessibleChildByID(docAcc, "fNode");
    const dpr = await getContentDPR(browser);
    const [, , containerWidth] = Layout.getBounds(container, dpr);
    const [, , aNodeWidth] = Layout.getBounds(aNode, dpr);

    await testChildAtPoint(
      dpr,
      containerWidth - 1,
      1,
      container,
      aNode,
      aNode.firstChild
    );
    await testChildAtPoint(
      dpr,
      containerWidth - aNodeWidth - 1,
      1,
      container,
      fNode,
      fNode.firstChild
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

/**
 * Verify that hit testing is appropriately fuzzy when working with generics.
 * If we match on a generic which contains additional generics and a single text
 * leaf, we should return the text leaf as the deepest match instead of the
 * generic itself.
 */
addAccessibleTask(
  `
  <a href="example.com" id="link">
    <span style="overflow:hidden;" id="generic"><span aria-hidden="true" id="visible">I am some visible text</span><span id="invisible" style="overflow:hidden; height: 1px; width: 1px; position:absolute; clip: rect(0 0 0 0); display:block;">I am some invisible text</span></span>
  </a>`,
  async function (browser, docAcc) {
    const link = findAccessibleChildByID(docAcc, "link");
    const generic = findAccessibleChildByID(docAcc, "generic");
    const invisible = findAccessibleChildByID(docAcc, "invisible");
    const dpr = await getContentDPR(browser);

    await testChildAtPoint(
      dpr,
      1,
      1,
      link,
      generic, // Direct Child
      invisible.firstChild // Deepest Child
    );

    await testOffsetAtPoint(
      findAccessibleChildByID(docAcc, "invisible", [Ci.nsIAccessibleText]),
      1,
      1,
      COORDTYPE_PARENT_RELATIVE,
      0
    );
  },
  { chrome: false, iframe: true, remoteIframe: true }
);

/**
 * Verify that hit testing is appropriately fuzzy when working with generics with siblings.
 * We should return the deepest text leaf as the deepest match instead of the generic itself.
 */
addAccessibleTask(
  `
<div id="generic"><span aria-hidden="true" id="visible">Mozilla</span><span id="invisible" style="display: block !important;border: 0 !important;clip: rect(0 0 0 0) !important;height: 1px !important;margin: -1px !important;overflow: hidden !important;padding: 0 !important;position: absolute !important;white-space: nowrap !important;width: 1px !important;">hello world<br><div id="extraContainer">Mozilla</div></span><br>I am some other text</div>`,
  async function (browser, docAcc) {
    const generic = findAccessibleChildByID(docAcc, "generic");
    const invisible = findAccessibleChildByID(docAcc, "invisible");
    const dpr = await getContentDPR(browser);

    await testChildAtPoint(
      dpr,
      1,
      1,
      generic,
      invisible, // Direct Child
      invisible.firstChild // Deepest Child
    );
  },
  { chrome: false, iframe: true, remoteIframe: true }
);

/**
 * Verify that hit testing correctly ignores
 * elements with pointer-events: none;
 */
addAccessibleTask(
  `<div id="container" style="position:relative;"><button id="obscured">click me</button><div id="overlay" style="pointer-events:none; top:0; bottom:0; left:0; right:0; position: absolute;"></div></div><button id="clickable">I am clickable</button>`,
  async function (browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "container");
    const obscured = findAccessibleChildByID(docAcc, "obscured");
    const clickable = findAccessibleChildByID(docAcc, "clickable");
    const dpr = await getContentDPR(browser);
    let [targetX, targetY, targetW, targetH] = Layout.getBounds(obscured, dpr);
    const [x, y] = Layout.getBounds(docAcc, dpr);
    await testChildAtPoint(
      dpr,
      targetX - x + targetW / 2,
      targetY - y + targetH / 2,
      docAcc,
      container, // Direct Child
      obscured // Deepest Child
    );

    [targetX, targetY, targetW, targetH] = Layout.getBounds(clickable, dpr);
    await testChildAtPoint(
      dpr,
      targetX - x + targetW / 2,
      targetY - y + targetH / 2,
      docAcc,
      clickable, // Direct Child
      clickable // Deepest Child
    );
  },
  { chrome: false, iframe: true, remoteIframe: true }
);
