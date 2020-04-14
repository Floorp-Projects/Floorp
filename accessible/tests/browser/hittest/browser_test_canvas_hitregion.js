/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function convertCSSToDevicePixels(browser, ...bounds) {
  return invokeContentTask(browser, bounds, (x, y, width, height) => {
    const { Layout: LayoutUtils } = ChromeUtils.import(
      "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
    );

    return LayoutUtils.CSSToDevicePixels(content, x, y, width, height);
  });
}

async function runTests(browser, accDoc) {
  Services.prefs.setBoolPref("canvas.hitregions.enabled", true);

  const offsetX = 20;
  const offsetY = 40;
  await invokeContentTask(browser, [], () =>
    content.document.getElementById("hitcanvas").scrollIntoView(true)
  );
  const dpr = await getContentDPR(browser);

  await invokeContentTask(browser, [offsetX, offsetY], (x, y) => {
    const doc = content.document;
    const element = doc.getElementById("hitcheck");
    const context = doc.getElementById("hitcanvas").getContext("2d");
    context.save();
    context.font = "10px sans-serif";
    context.textAlign = "left";
    context.textBaseline = "middle";
    const metrics = context.measureText(element.parentNode.textContent);
    context.beginPath();
    context.strokeStyle = "black";
    context.rect(x - 5, y - 5, 10, 10);
    context.stroke();
    if (element.checked) {
      context.fillStyle = "black";
      context.fill();
    }

    context.fillText(element.parentNode.textContent, x + 5, y);
    context.beginPath();
    context.rect(x - 7, y - 7, 12 + metrics.width + 2, 14);

    if (doc.activeElement == element) {
      context.drawFocusIfNeeded(element);
    }

    context.addHitRegion({ control: element });
    context.restore();
  });

  const hitcanvas = findAccessibleChildByID(accDoc, "hitcanvas");
  const hitcheck = findAccessibleChildByID(accDoc, "hitcheck");
  const [hitX, hitY /* hitWidth, hitHeight */] = Layout.getBounds(
    hitcanvas,
    dpr
  );
  const [deltaX, deltaY] = await convertCSSToDevicePixels(
    browser,
    offsetX,
    offsetY
  );

  info("Test if we hit the region associated with the shadow dom checkbox.");
  const tgtX = hitX + deltaX;
  let tgtY = hitY + deltaY;
  let hitAcc = accDoc.getDeepestChildAtPoint(tgtX, tgtY);
  CommonUtils.isObject(hitAcc, hitcheck, `Hit match at (${tgtX}, ${tgtY}`);

  info(
    "Test that we don't hit the region associated with the shadow dom checkbox."
  );
  tgtY = hitY + deltaY * 2;
  hitAcc = accDoc.getDeepestChildAtPoint(tgtX, tgtY);
  CommonUtils.isObject(hitAcc, hitcanvas, `Hit match at (${tgtX}, ${tgtY}`);

  Services.prefs.clearUserPref("canvas.hitregions.enabled");
}

addAccessibleTask(
  `
  <canvas id="hitcanvas">
    <input id="hitcheck" type="checkbox"><label for="showA"> Show A </label>
  </canvas>`,
  runTests,
  { iframe: true, remoteIframe: true }
);
