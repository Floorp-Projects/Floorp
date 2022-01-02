/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Adapted from browser_canvas_fingerprinting_resistance.js
 */
"use strict";

const kUrl = "https://example.com/";

function initTab() {
  let contentWindow = content.wrappedJSObject;

  let drawCanvas = (fillStyle, id) => {
    let contentDocument = contentWindow.document;
    let width = 64,
      height = 64;
    let canvas = contentDocument.createElement("canvas");
    if (id) {
      canvas.setAttribute("id", id);
    }
    canvas.setAttribute("width", width);
    canvas.setAttribute("height", height);
    contentDocument.body.appendChild(canvas);

    let context = canvas.getContext("2d");
    context.fillStyle = fillStyle;
    context.fillRect(0, 0, width, height);
    return canvas;
  };

  let canvas = drawCanvas("cyan", "canvas-id-canvas");
  contentWindow.kPlacedData = canvas.toDataURL();
  is(
    canvas.toDataURL(),
    contentWindow.kPlacedData,
    "no RFP, canvas data == placed data"
  );
}

function disableResistFingerprinting() {
  return SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", false]],
  });
}

function enableResistFingerprinting(RfpNonPbmExclusion, RfpDomainExclusion) {
  if (RfpNonPbmExclusion && RfpDomainExclusion) {
    return SpecialPowers.pushPrefEnv({
      set: [
        ["privacy.resistFingerprinting", true],
        ["privacy.resistFingerprinting.testGranularityMask", 6],
        ["privacy.resistFingerprinting.exemptedDomains", "example.com"],
      ],
    });
  } else if (RfpNonPbmExclusion) {
    return SpecialPowers.pushPrefEnv({
      set: [
        ["privacy.resistFingerprinting", true],
        ["privacy.resistFingerprinting.testGranularityMask", 2],
      ],
    });
  } else if (RfpDomainExclusion) {
    return SpecialPowers.pushPrefEnv({
      set: [
        ["privacy.resistFingerprinting", true],
        ["privacy.resistFingerprinting.testGranularityMask", 4],
        ["privacy.resistFingerprinting.exemptedDomains", "example.com"],
      ],
    });
  }
  return SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true],
      ["privacy.resistFingerprinting.testGranularityMask", 0],
    ],
  });
}

function extractCanvasData(isPbm, RfpNonPbmExclusion, RfpDomainExclusion) {
  let contentWindow = content.wrappedJSObject;
  let canvas = contentWindow.document.getElementById("canvas-id-canvas");
  let canvasData = canvas.toDataURL();

  if (RfpDomainExclusion) {
    is(
      canvasData,
      contentWindow.kPlacedData,
      `RFP, domain exempted, canvas data == placed data (isPbm: ${isPbm}, RfpNonPbmExclusion: ${RfpNonPbmExclusion}, RfpDomainExclusion: ${RfpDomainExclusion})`
    );
  } else if (!isPbm && RfpNonPbmExclusion) {
    is(
      canvasData,
      contentWindow.kPlacedData,
      `RFP, nonPBM exempted, not in PBM, canvas data == placed data (isPbm: ${isPbm}, RfpNonPbmExclusion: ${RfpNonPbmExclusion}, RfpDomainExclusion: ${RfpDomainExclusion})`
    );
  } else if (isPbm && RfpNonPbmExclusion) {
    isnot(
      canvasData,
      contentWindow.kPlacedData,
      `RFP, nonPBM exempted, in PBM, canvas data != placed data (isPbm: ${isPbm}, RfpNonPbmExclusion: ${RfpNonPbmExclusion}, RfpDomainExclusion: ${RfpDomainExclusion})`
    );
  } else {
    isnot(
      canvasData,
      contentWindow.kPlacedData,
      `RFP, domain not exempted, nonPBM not exempted, canvas data != placed data (isPbm: ${isPbm}, RfpNonPbmExclusion: ${RfpNonPbmExclusion}, RfpDomainExclusion: ${RfpDomainExclusion})`
    );
  }
}

async function rfpExclusionTestOnCanvas(
  win,
  isPbm,
  RfpNonPbmExclusion,
  RfpDomainExclusion
) {
  let browser = win.gBrowser.selectedBrowser;
  await disableResistFingerprinting(); /* we need to disable RFP to capture the placed data */
  await SpecialPowers.spawn(browser, [], initTab);
  await enableResistFingerprinting(RfpNonPbmExclusion, RfpDomainExclusion);
  await SpecialPowers.spawn(
    browser,
    [isPbm, RfpNonPbmExclusion, RfpDomainExclusion],
    extractCanvasData
  );
}

async function testCanvasRfpExclusion(
  isPbm,
  RfpNonPbmExclusion,
  RfpDomainExclusion
) {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: isPbm,
  });
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: win.gBrowser,
      url: kUrl,
    },
    rfpExclusionTestOnCanvas.bind(
      null,
      win,
      isPbm,
      RfpNonPbmExclusion,
      RfpDomainExclusion
    )
  );
  await BrowserTestUtils.closeWindow(win);
}

add_task(testCanvasRfpExclusion.bind(null, false, false, false));
add_task(testCanvasRfpExclusion.bind(null, false, false, true));
add_task(testCanvasRfpExclusion.bind(null, false, true, false));
add_task(testCanvasRfpExclusion.bind(null, false, true, true));
add_task(testCanvasRfpExclusion.bind(null, true, false, false));
add_task(testCanvasRfpExclusion.bind(null, true, false, true));
add_task(testCanvasRfpExclusion.bind(null, true, true, false));
add_task(testCanvasRfpExclusion.bind(null, true, true, true));
