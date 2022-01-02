/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function runTests(browser, accDoc) {
  await waitForImageMap(browser, accDoc);
  const dpr = await getContentDPR(browser);

  info("Not specific case, child and deepchild testing.");
  testChildAtPoint(
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

  info(
    "::MustPrune case (in this case childAtPoint doesn't look inside a " +
      "textbox), point is inside of textbox."
  );
  const txt = findAccessibleChildByID(accDoc, "txt");
  testChildAtPoint(dpr, 1, 1, txt, txt, txt);

  info(
    "::MustPrune case, point is outside of textbox accessible but is in document."
  );
  testChildAtPoint(dpr, -1, -1, txt, null, null);

  info("::MustPrune case, point is outside of root accessible.");
  testChildAtPoint(dpr, -10000, -10000, txt, null, null);

  info("Not specific case, point is inside of btn accessible.");
  const btn = findAccessibleChildByID(accDoc, "btn");
  testChildAtPoint(dpr, 1, 1, btn, btn, btn);

  info("Not specific case, point is outside of btn accessible.");
  testChildAtPoint(dpr, -1, -1, btn, null, null);

  info(
    "Out of flow accessible testing, do not return out of flow accessible " +
      "because it's not a child of the accessible even though visually it is."
  );
  await invokeContentTask(browser, [], () => {
    const { CommonUtils } = ChromeUtils.import(
      "chrome://mochitests/content/browser/accessible/tests/browser/Common.jsm"
    );

    const doc = content.document;
    const rectArea = CommonUtils.getNode("area", doc).getBoundingClientRect();
    const outOfFlow = CommonUtils.getNode("outofflow", doc);
    outOfFlow.style.left = rectArea.left + "px";
    outOfFlow.style.top = rectArea.top + "px";
  });

  const area = findAccessibleChildByID(accDoc, "area");
  testChildAtPoint(dpr, 1, 1, area, area, area);

  info("Test image maps. Their children are not in the layout tree.");
  const imgmap = findAccessibleChildByID(accDoc, "imgmap");
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
  `,
  runTests,
  {
    iframe: true,
    remoteIframe: true,
    // Ensure that all hittest elements are in view.
    iframeAttrs: { style: "width: 600px; height: 600px;" },
  }
);
