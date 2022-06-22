/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Layout } = ChromeUtils.import(
  "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
);

const { CommonUtils } = ChromeUtils.import(
  "chrome://mochitests/content/browser/accessible/tests/browser/Common.jsm"
);

function getChildAtPoint(container, x, y, findDeepestChild) {
  try {
    const child = findDeepestChild
      ? container.getDeepestChildAtPoint(x, y)
      : container.getChildAtPoint(x, y);
    info(`Got child with role: ${roleToString(child.role)}`);
    return child;
  } catch (e) {
    // Failed to get child at point.
  }

  return null;
}

async function testChildAtPoint(dpr, x, y, container, child, grandChild) {
  const [containerX, containerY] = Layout.getBounds(container, dpr);
  x += containerX;
  y += containerY;
  await untilCacheIs(
    () => getChildAtPoint(container, x, y, false),
    child,
    `Wrong direct child accessible at the point (${x}, ${y}) of ${CommonUtils.prettyName(
      container
    )}, sought ${child ? roleToString(child.role) : "unknown"}`
  );
  await untilCacheIs(
    () => getChildAtPoint(container, x, y, true),
    grandChild,
    `Wrong deepest child accessible at the point (${x}, ${y}) of ${CommonUtils.prettyName(
      container
    )}, sought ${grandChild ? roleToString(grandChild.role) : "unknown"}`
  );
}

async function hitTest(browser, container, child, grandChild) {
  const [childX, childY] = await getContentBoundsForDOMElm(
    browser,
    getAccessibleDOMNodeID(child)
  );
  const x = childX + 1;
  const y = childY + 1;

  await untilCacheIs(
    () => getChildAtPoint(container, x, y, false),
    child,
    `Wrong direct child accessible at the point (${x}, ${y}) of ${CommonUtils.prettyName(
      container
    )}, sought ${child ? roleToString(child.role) : "unknown"}`
  );
  await untilCacheIs(
    () => getChildAtPoint(container, x, y, true),
    grandChild,
    `Wrong deepest child accessible at the point (${x}, ${y}) of ${CommonUtils.prettyName(
      container
    )}, sought ${grandChild ? roleToString(grandChild.role) : "unknown"}`
  );
}

async function runTests(browser, accDoc) {
  await waitForImageMap(browser, accDoc);
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
    // We have to reimprot CommonUtils in this scope -- eslint thinks this is
    // wrong, but if you remove it, things will break.
    /* eslint-disable no-shadow */
    const { CommonUtils } = ChromeUtils.import(
      "chrome://mochitests/content/browser/accessible/tests/browser/Common.jsm"
    );
    /* eslint-enable no-shadow */
    const doc = content.document;
    const rectArea = CommonUtils.getNode("area", doc).getBoundingClientRect();
    const outOfFlow = CommonUtils.getNode("outofflow", doc);
    outOfFlow.style.left = rectArea.left + "px";
    outOfFlow.style.top = rectArea.top + "px";
  });

  const area = findAccessibleChildByID(accDoc, "area");
  await testChildAtPoint(dpr, 1, 1, area, area, area);

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
    iframeAttrs: { style: "width: 600px; height: 600px; padding: 10px;" },
  }
);
