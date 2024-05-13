/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// From https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/ne-uiautomationcore-livesetting
const LiveSetting = {
  Off: 0,
  Polite: 1,
  Assertive: 2,
};

/**
 * Test the LiveSetting property.
 */
addUiaTask(
  `
<div id="polite" aria-live="polite">
  <div id="inner">polite</div>
</div>
<div id="assertive" aria-live="assertive">assertive</div>
<div id="off" aria-live="off">off</div>
<output id="output">output</output>
<div id="none">none</div>
  `,
  async function testLiveSettingProp() {
    await definePyVar("doc", `getDocUia()`);
    is(
      await runPython(`findUiaByDomId(doc, "polite").CurrentLiveSetting`),
      LiveSetting.Polite,
      "polite has correct LiveSetting"
    );
    // LiveSetting should only be exposed on the root of a live region.
    // The IA2 -> UIA proxy disagrees, but:
    // 1. The UIA documentation doesn't specify whether descendants should
    // expose this.
    // 2. Chromium only exposes it on the root. Given that live regions work in
    // Chromium but not with the IA2 -> UIA proxy, it makes sense to follow
    // Chromium in the absence of good documentation.
    // 3. It's cheaper to expose it only on the root, since that avoids many
    // ancestor walks.
    if (gIsUiaEnabled) {
      is(
        await runPython(`findUiaByDomId(doc, "inner").CurrentLiveSetting`),
        LiveSetting.Off,
        "inner has correct LiveSetting"
      );
    }
    is(
      await runPython(`findUiaByDomId(doc, "assertive").CurrentLiveSetting`),
      LiveSetting.Assertive,
      "assertive has correct LiveSetting"
    );
    is(
      await runPython(`findUiaByDomId(doc, "off").CurrentLiveSetting`),
      LiveSetting.Off,
      "off has correct LiveSetting"
    );
    is(
      await runPython(`findUiaByDomId(doc, "output").CurrentLiveSetting`),
      LiveSetting.Polite,
      "output has correct LiveSetting"
    );
    is(
      await runPython(`findUiaByDomId(doc, "none").CurrentLiveSetting`),
      LiveSetting.Off,
      "none has correct LiveSetting"
    );
  }
);

/**
 * Test exposure of aria-atomic via the AriaProperties property.
 */
addUiaTask(
  `
<div id="implicit" aria-live="polite">live</div>
<div id="false" aria-live="polite" aria-atomic="false">false</div>
<div id="true" aria-live="polite" aria-atomic="true">true</div>
<div id="none">none</div>
  `,
  async function testAtomic() {
    await definePyVar("doc", `getDocUia()`);
    let result = await runPython(
      `findUiaByDomId(doc, "implicit").CurrentAriaProperties`
    );
    isnot(
      result.indexOf("atomic=false"),
      -1,
      "AriaProperties for implicit contains atomic=false"
    );
    result = await runPython(
      `findUiaByDomId(doc, "false").CurrentAriaProperties`
    );
    isnot(
      result.indexOf("atomic=false"),
      -1,
      "AriaProperties for false contains atomic=false"
    );
    result = await runPython(
      `findUiaByDomId(doc, "true").CurrentAriaProperties`
    );
    isnot(
      result.indexOf("atomic=true"),
      -1,
      "AriaProperties for true contains atomic=true"
    );
    result = await runPython(
      `findUiaByDomId(doc, "none").CurrentAriaProperties`
    );
    is(
      result.indexOf("atomic"),
      -1,
      "AriaProperties for none doesn't contain atomic"
    );
  },
  // The IA2 -> UIA proxy doesn't support atomic.
  { uiaEnabled: true, uiaDisabled: false }
);

/**
 * Test that a live region is exposed as a control element.
 */
addUiaTask(
  `
<div id="live" aria-live="polite">
  <div id="inner">live</div>
</div>
<div id="notLive">notLive</div>
  `,
  async function testIsControl() {
    await definePyVar("doc", `getDocUia()`);
    ok(
      await runPython(`findUiaByDomId(doc, "live").CurrentIsControlElement`),
      "live is a control element"
    );
    // The IA2 -> UIA proxy gets this wrong.
    if (gIsUiaEnabled) {
      ok(
        !(await runPython(
          `findUiaByDomId(doc, "inner").CurrentIsControlElement`
        )),
        "inner is not a control element"
      );
    }
    ok(
      !(await runPython(
        `findUiaByDomId(doc, "notLive").CurrentIsControlElement`
      )),
      "notLive is not a control element"
    );
  }
);

/**
 * Test LiveRegionChanged events.
 */
addUiaTask(
  `
<div id="live" aria-live="polite">
  a
  <div id="b" hidden>b</div>
  <span id="c" hidden role="none">c</span>
  <button id="d" aria-label="before">button</button>
</div>
  `,
  async function testLiveRegionChangedEvent(browser) {
    await definePyVar("doc", `getDocUia()`);
    info("Showing b");
    await setUpWaitForUiaEvent("LiveRegionChanged", "live");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("b").hidden = false;
    });
    await waitForUiaEvent();
    ok(true, "Got LiveRegionChanged on live");

    // The c span doesn't get an Accessible, so this tests that we get an event
    // when a text leaf is added directly.
    info("Showing c");
    await setUpWaitForUiaEvent("LiveRegionChanged", "live");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("c").hidden = false;
    });
    await waitForUiaEvent();
    ok(true, "Got LiveRegionChanged on live");

    info("Setting d's aria-label");
    await setUpWaitForUiaEvent("LiveRegionChanged", "live");
    await invokeSetAttribute(browser, "d", "aria-label", "d");
    await waitForUiaEvent();
    ok(true, "Got LiveRegionChanged on live");

    info("Setting live textContent (new text leaf)");
    await setUpWaitForUiaEvent("LiveRegionChanged", "live");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("live").textContent = "replaced";
    });
    await waitForUiaEvent();
    ok(true, "Got LiveRegionChanged on live");

    info("Mutating live's text node (same text leaf)");
    await setUpWaitForUiaEvent("LiveRegionChanged", "live");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("live").firstChild.data = "again";
    });
    await waitForUiaEvent();
    ok(true, "Got LiveRegionChanged on live");
  },
  // The IA2 -> UIA proxy doesn't fire LiveRegionChanged.
  { uiaEnabled: true, uiaDisabled: false }
);
