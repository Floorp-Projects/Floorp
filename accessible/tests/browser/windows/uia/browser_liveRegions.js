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
