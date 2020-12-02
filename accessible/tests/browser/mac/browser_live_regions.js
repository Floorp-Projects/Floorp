/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test live region creation and removal.
 */
addAccessibleTask(
  `
  <div id="polite" aria-relevant="removals">Polite region</div>
  <div id="assertive" aria-live="assertive">Assertive region</div>
  `,
  async (browser, accDoc) => {
    let politeRegion = getNativeInterface(accDoc, "polite");
    ok(
      !politeRegion.attributeNames.includes("AXARIALive"),
      "region is not live"
    );

    let liveRegionAdded = waitForMacEvent("AXLiveRegionCreated", "polite");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("polite")
        .setAttribute("aria-atomic", "true");
      content.document
        .getElementById("polite")
        .setAttribute("aria-live", "polite");
    });
    await liveRegionAdded;
    is(
      politeRegion.getAttributeValue("AXARIALive"),
      "polite",
      "region is now live"
    );
    ok(politeRegion.getAttributeValue("AXARIAAtomic"), "region is atomic");
    is(
      politeRegion.getAttributeValue("AXARIARelevant"),
      "removals",
      "region has defined aria-relevant"
    );

    let assertiveRegion = getNativeInterface(accDoc, "assertive");
    is(
      assertiveRegion.getAttributeValue("AXARIALive"),
      "assertive",
      "region is assertive"
    );
    ok(
      !assertiveRegion.getAttributeValue("AXARIAAtomic"),
      "region is not atomic"
    );
    is(
      assertiveRegion.getAttributeValue("AXARIARelevant"),
      "additions text",
      "region has default aria-relevant"
    );

    let liveRegionRemoved = waitForEvent(
      EVENT_LIVE_REGION_REMOVED,
      "assertive"
    );
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("assertive").removeAttribute("aria-live");
    });
    await liveRegionRemoved;
    ok(!assertiveRegion.getAttributeValue("AXARIALive"), "region is not live");

    liveRegionAdded = waitForMacEvent("AXLiveRegionCreated", "new-region");
    await SpecialPowers.spawn(browser, [], () => {
      let newRegionElm = content.document.createElement("div");
      newRegionElm.id = "new-region";
      newRegionElm.setAttribute("aria-live", "assertive");
      content.document.body.appendChild(newRegionElm);
    });
    await liveRegionAdded;

    let newRegion = getNativeInterface(accDoc, "new-region");
    is(
      newRegion.getAttributeValue("AXARIALive"),
      "assertive",
      "region is assertive"
    );

    let loadComplete = Promise.all([
      waitForMacEvent("AXLoadComplete"),
      waitForMacEvent("AXLiveRegionCreated", "region-1"),
      waitForMacEvent("AXLiveRegionCreated", "region-2"),
      waitForMacEvent("AXLiveRegionCreated", "status"),
      waitForMacEvent("AXLiveRegionCreated", "output"),
    ]);

    await SpecialPowers.spawn(browser, [], () => {
      content.location = `data:text/html;charset=utf-8,
        <div id="region-1" aria-live="polite"></div>
        <div id="region-2" aria-live="assertive"></div>
        <div id="region-3" aria-live="off"></div>
        <div id="alert" role="alert"></div>
        <div id="status" role="status"></div>
        <output id="output"></output>`;
    });
    let webArea = (await loadComplete)[0];

    is(webArea.getAttributeValue("AXRole"), "AXWebArea", "web area yeah");
    const searchPred = {
      AXSearchKey: "AXLiveRegionSearchKey",
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };
    const liveRegions = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    Assert.deepEqual(
      liveRegions.map(r => r.getAttributeValue("AXDOMIdentifier")),
      ["region-1", "region-2", "alert", "status", "output"],
      "SearchPredicate returned all live regions"
    );
  }
);

/**
 * Test live region changes
 */
addAccessibleTask(
  `
  <div id="live" aria-live="polite">
  The time is <span id="time">4:55pm</span>
  <p id="p" style="display: none">Georgia on my mind</p>
  <button id="button" aria-label="Start"></button>
  </div>
  `,
  async (browser, accDoc) => {
    let liveRegionChanged = waitForMacEvent("AXLiveRegionChanged", "live");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("time").textContent = "4:56pm";
    });
    await liveRegionChanged;
    ok(true, "changed textContent");

    liveRegionChanged = waitForMacEvent("AXLiveRegionChanged", "live");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("p").style.display = "block";
    });
    await liveRegionChanged;
    ok(true, "changed display style to block");

    liveRegionChanged = waitForMacEvent("AXLiveRegionChanged", "live");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("p").style.display = "none";
    });
    await liveRegionChanged;
    ok(true, "changed display style to none");

    liveRegionChanged = waitForMacEvent("AXLiveRegionChanged", "live");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("button")
        .setAttribute("aria-label", "Stop");
    });
    await liveRegionChanged;
    ok(true, "changed aria-label");
  }
);
