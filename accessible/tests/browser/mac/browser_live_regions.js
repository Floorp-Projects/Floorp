/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test live region creation and removal.
 */
addAccessibleTask(
  `
  <div id="polite">Polite region</div>
  <div id="assertive" aria-live="assertive">Assertive region</div>
  `,
  async (browser, accDoc) => {
    let liveRegionAdded = waitForEvent(EVENT_LIVE_REGION_ADDED, "polite");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("polite")
        .setAttribute("aria-atomic", "true");
      content.document
        .getElementById("polite")
        .setAttribute("aria-live", "polite");
    });
    await liveRegionAdded;

    let liveRegionRemoved = waitForEvent(
      EVENT_LIVE_REGION_REMOVED,
      "assertive"
    );
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("assertive").removeAttribute("aria-live");
    });
    await liveRegionRemoved;

    liveRegionAdded = waitForEvent(EVENT_LIVE_REGION_ADDED, "new-region");
    await SpecialPowers.spawn(browser, [], () => {
      let newRegionElm = content.document.createElement("div");
      newRegionElm.id = "new-region";
      newRegionElm.setAttribute("aria-live", "assertive");
      content.document.body.appendChild(newRegionElm);
    });
    await liveRegionAdded;

    let loadComplete = Promise.all([
      waitForMacEvent("AXLoadComplete"),
      waitForEvent(EVENT_LIVE_REGION_ADDED, "region-1"),
      waitForEvent(EVENT_LIVE_REGION_ADDED, "region-2"),
      waitForEvent(EVENT_LIVE_REGION_ADDED, "status"),
      waitForEvent(EVENT_LIVE_REGION_ADDED, "output"),
    ]);

    await SpecialPowers.spawn(browser, [], () => {
      content.location = `data:text/html;charset=utf-8,
        <div id="region-1" aria-live="polite"></div>
        <div id="region-2" aria-live="assertive"></div>
        <div id="region-3" aria-live="off"></div>
        <div id="status" role="status"></div>
        <output id="output"></output>`;
    });
    await loadComplete;

    liveRegionRemoved = waitForEvent(EVENT_LIVE_REGION_REMOVED, "status");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("status")
        .setAttribute("aria-live", "off");
    });
    await liveRegionRemoved;

    liveRegionRemoved = waitForEvent(EVENT_LIVE_REGION_REMOVED, "output");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("output")
        .setAttribute("aria-live", "off");
    });
    await liveRegionRemoved;
  }
);
