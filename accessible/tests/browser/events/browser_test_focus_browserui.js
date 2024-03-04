/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/states.js */
/* import-globals-from ../../mochitest/role.js */
loadScripts(
  { name: "states.js", dir: MOCHITESTS_DIR },
  { name: "role.js", dir: MOCHITESTS_DIR }
);

async function runTests(browser) {
  await SpecialPowers.pushPrefEnv({
    // If Fission is disabled, the pref is no-op.
    set: [["fission.bfcacheInParent", true]],
  });

  let onFocus = waitForEvent(EVENT_FOCUS, "input");
  EventUtils.synthesizeKey("VK_TAB", {}, browser.ownerGlobal);
  let evt = await onFocus;
  testStates(evt.accessible, STATE_FOCUSED);

  onFocus = waitForEvent(EVENT_FOCUS, "buttonInputDoc");
  let url = snippetToURL(`<input id="input" type="button" value="button">`, {
    contentDocBodyAttrs: { id: "buttonInputDoc" },
  });
  browser.loadURI(Services.io.newURI(url), {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  evt = await onFocus;
  testStates(evt.accessible, STATE_FOCUSED);

  onFocus = waitForEvent(EVENT_FOCUS, "input");
  browser.goBack();
  evt = await onFocus;
  testStates(evt.accessible, STATE_FOCUSED);

  onFocus = waitForEvent(
    EVENT_FOCUS,
    event => event.accessible.DOMNode == gURLBar.inputField
  );
  EventUtils.synthesizeKey("t", { accelKey: true }, browser.ownerGlobal);
  evt = await onFocus;
  testStates(evt.accessible, STATE_FOCUSED);

  onFocus = waitForEvent(EVENT_FOCUS, "input");
  EventUtils.synthesizeKey("w", { accelKey: true }, browser.ownerGlobal);
  evt = await onFocus;
  testStates(evt.accessible, STATE_FOCUSED);
}

/**
 * Accessibility loading document events test.
 */
addAccessibleTask(`<input id="input">`, runTests);
