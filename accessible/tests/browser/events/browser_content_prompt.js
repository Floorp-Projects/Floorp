/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Want to test relations.
/* import-globals-from ../../mochitest/name.js */
/* import-globals-from ../../mochitest/relations.js */
/* import-globals-from ../../mochitest/role.js */
loadScripts(
  { name: "relations.js", dir: MOCHITESTS_DIR },
  { name: "name.js", dir: MOCHITESTS_DIR },
  { name: "role.js", dir: MOCHITESTS_DIR }
);

addAccessibleTask(``, async function (browser) {
  info("Showing alert");
  let shown = waitForEvent(
    EVENT_SHOW,
    evt => evt.accessible.role == ROLE_INTERNAL_FRAME
  );
  // Let's make sure the dialog content gets focus.
  // On macOS, we unfortunately focus the label. We focus the OK button on
  // all other platforms. See https://phabricator.services.mozilla.com/D204908
  // for more discussion.
  let expectedRole =
    AppConstants.platform == "macosx" ? ROLE_LABEL : ROLE_PUSHBUTTON;
  let focused = waitForEvent(EVENT_FOCUS, evt => {
    return evt.accessible.role == expectedRole;
  });
  await invokeContentTask(browser, [], () => {
    // Use setTimeout to avoid blocking the return of the content task
    // on the alert, which is otherwise synchronous.
    content.setTimeout(() => content.alert("test"), 0);
  });
  const frame = (await shown).accessible;
  const focusedEl = (await focused).accessible;
  ok(true, "Dialog shown and something got focused");
  let dialog = getAccessible(focusedEl.DOMNode.ownerDocument);
  testRole(dialog, ROLE_DIALOG);
  let infoBody = focusedEl.DOMNode.ownerDocument.getElementById("infoBody");
  testRelation(dialog, RELATION_DESCRIBED_BY, infoBody);
  testDescr(dialog, "test  ");
  info("Dismissing alert");
  let hidden = waitForEvent(EVENT_HIDE, frame);
  EventUtils.synthesizeKey("KEY_Escape", {}, frame.DOMNode.contentWindow);
  await hidden;
});
