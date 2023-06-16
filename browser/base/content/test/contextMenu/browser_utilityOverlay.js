/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_eventMatchesKey() {
  let eventMatchResult;
  let key;
  let checkEvent = function (e) {
    e.stopPropagation();
    e.preventDefault();
    eventMatchResult = eventMatchesKey(e, key);
  };
  document.addEventListener("keypress", checkEvent);

  try {
    key = document.createXULElement("key");
    let keyset = document.getElementById("mainKeyset");
    key.setAttribute("key", "t");
    key.setAttribute("modifiers", "accel");
    keyset.appendChild(key);
    EventUtils.synthesizeKey("t", { accelKey: true });
    is(eventMatchResult, true, "eventMatchesKey: one modifier");
    keyset.removeChild(key);

    key = document.createXULElement("key");
    key.setAttribute("key", "g");
    key.setAttribute("modifiers", "accel,shift");
    keyset.appendChild(key);
    EventUtils.synthesizeKey("g", { accelKey: true, shiftKey: true });
    is(eventMatchResult, true, "eventMatchesKey: combination modifiers");
    keyset.removeChild(key);

    key = document.createXULElement("key");
    key.setAttribute("key", "w");
    key.setAttribute("modifiers", "accel");
    keyset.appendChild(key);
    EventUtils.synthesizeKey("f", { accelKey: true });
    is(eventMatchResult, false, "eventMatchesKey: mismatch keys");
    keyset.removeChild(key);

    key = document.createXULElement("key");
    key.setAttribute("keycode", "VK_DELETE");
    keyset.appendChild(key);
    EventUtils.synthesizeKey("VK_DELETE", { accelKey: true });
    is(eventMatchResult, false, "eventMatchesKey: mismatch modifiers");
    keyset.removeChild(key);
  } finally {
    // Make sure to remove the event listener so future tests don't
    // fail when they simulate key presses.
    document.removeEventListener("keypress", checkEvent);
  }
});

add_task(async function test_getTargetWindow() {
  is(URILoadingHelper.getTargetWindow(window), window, "got top window");
});

add_task(async function test_openUILink() {
  const kURL = "https://example.org/";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let loadPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    kURL
  );

  openUILink(kURL, null, {
    triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
  }); // defaults to "current"

  await loadPromise;

  is(tab.linkedBrowser.currentURI.spec, kURL, "example.org loaded");
  gBrowser.removeCurrentTab();
});
