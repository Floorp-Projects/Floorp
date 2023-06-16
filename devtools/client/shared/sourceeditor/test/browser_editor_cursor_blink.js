/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test to make sure that the editor reacts to preference changes

const CARET_BLINK_TIME = "ui.caretBlinkTime";

add_task(async function () {
  Services.prefs.clearUserPref(CARET_BLINK_TIME);

  info(`Test when "${CARET_BLINK_TIME}" isn't set`);
  let { ed, win } = await setup();
  checkCssCustomPropertyValue(
    ed,
    530,
    "When preference isn't set, blink time is set to codeMirror default"
  );
  ed.destroy();
  win.close();

  info(`Test with a positive value for "${CARET_BLINK_TIME}"`);
  let blinkTime = 200;
  Services.prefs.setIntPref(CARET_BLINK_TIME, blinkTime);
  ({ ed, win } = await setup());

  checkCssCustomPropertyValue(
    ed,
    blinkTime,
    "When preference is set, blink time reflects the pref value"
  );
  ed.destroy();
  win.close();

  info(`Test when "${CARET_BLINK_TIME}" is 0`);
  blinkTime = 0;
  Services.prefs.setIntPref(CARET_BLINK_TIME, blinkTime);
  ({ ed, win } = await setup());

  checkCssCustomPropertyValue(
    ed,
    blinkTime,
    "When preference value is 0, blink time is also 0"
  );
  ed.destroy();
  win.close();

  info(`Test when "${CARET_BLINK_TIME}" is -1`);
  blinkTime = -1;
  Services.prefs.setIntPref(CARET_BLINK_TIME, blinkTime);
  ({ ed, win } = await setup());

  checkCssCustomPropertyValue(
    ed,
    0,
    "When preference value is negative, blink time is 0"
  );
  ed.destroy();
  win.close();

  Services.prefs.clearUserPref(CARET_BLINK_TIME);
});

function checkCssCustomPropertyValue(editor, expectedMsValue, assertionText) {
  is(
    editor.codeMirror
      .getWrapperElement()
      .style.getPropertyValue("--caret-blink-time"),
    `${expectedMsValue}ms`,
    assertionText
  );
}
