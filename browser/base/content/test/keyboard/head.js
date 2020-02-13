/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Force focus to an element that isn't focusable.
 * Toolbar buttons aren't focusable because if they were, clicking them would
 * focus them, which is undesirable. Therefore, they're only made focusable
 * when a user is navigating with the keyboard. This function forces focus as
 * is done during toolbar keyboard navigation.
 */
function forceFocus(aElem) {
  aElem.setAttribute("tabindex", "-1");
  aElem.focus();
  aElem.removeAttribute("tabindex");
}

async function expectFocusAfterKey(
  aKey,
  aFocus,
  aAncestorOk = false,
  aWindow = window
) {
  let res = aKey.match(/^(Shift\+)?(?:(.)|(.+))$/);
  let shift = Boolean(res[1]);
  let key;
  if (res[2]) {
    key = res[2]; // Character.
  } else {
    key = "KEY_" + res[3]; // Tab, ArrowRight, etc.
  }
  let expected;
  let friendlyExpected;
  if (typeof aFocus == "string") {
    expected = aWindow.document.getElementById(aFocus);
    friendlyExpected = aFocus;
  } else {
    expected = aFocus;
    if (aFocus == aWindow.gURLBar.inputField) {
      friendlyExpected = "URL bar input";
    } else if (aFocus == aWindow.gBrowser.selectedBrowser) {
      friendlyExpected = "Web document";
    }
  }
  info("Listening on item " + (expected.id || expected.className));
  let focused = BrowserTestUtils.waitForEvent(expected, "focus", aAncestorOk);
  EventUtils.synthesizeKey(key, { shiftKey: shift }, aWindow);
  let receivedEvent = await focused;
  info(
    "Got focus on item: " +
      (receivedEvent.target.id || receivedEvent.target.className)
  );
  ok(true, friendlyExpected + " focused after " + aKey + " pressed");
}
