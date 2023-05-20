/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `
  <p id="p1">abc</p>
  <input id="input1" value="input" />`,
  async function (browser) {
    let onVCChanged = waitForEvent(
      EVENT_VIRTUALCURSOR_CHANGED,
      matchContentDoc
    );
    await invokeContentTask(browser, [], () => {
      const { CommonUtils } = ChromeUtils.importESModule(
        "chrome://mochitests/content/browser/accessible/tests/browser/Common.sys.mjs"
      );
      let vc = CommonUtils.getAccessible(
        content.document,
        Ci.nsIAccessibleDocument
      ).virtualCursor;
      vc.position = CommonUtils.getAccessible(
        "p1",
        null,
        null,
        null,
        content.document
      );
    });
    let vccEvent = (await onVCChanged).QueryInterface(
      nsIAccessibleVirtualCursorChangeEvent
    );
    is(vccEvent.newAccessible.id, "p1", "New position is correct");
    is(vccEvent.newStartOffset, -1, "New start offset is correct");
    is(vccEvent.newEndOffset, -1, "New end offset is correct");
    ok(!vccEvent.isFromUserInput, "not user initiated");

    onVCChanged = waitForEvent(EVENT_VIRTUALCURSOR_CHANGED, matchContentDoc);
    await invokeContentTask(browser, [], () => {
      const { CommonUtils } = ChromeUtils.importESModule(
        "chrome://mochitests/content/browser/accessible/tests/browser/Common.sys.mjs"
      );
      let vc = CommonUtils.getAccessible(
        content.document,
        Ci.nsIAccessibleDocument
      ).virtualCursor;
      vc.moveNextByText(Ci.nsIAccessiblePivot.CHAR_BOUNDARY);
    });
    vccEvent = (await onVCChanged).QueryInterface(
      nsIAccessibleVirtualCursorChangeEvent
    );
    is(vccEvent.newAccessible.id, vccEvent.oldAccessible.id, "Same position");
    is(vccEvent.newStartOffset, 0, "New start offset is correct");
    is(vccEvent.newEndOffset, 1, "New end offset is correct");
    ok(vccEvent.isFromUserInput, "user initiated");

    onVCChanged = waitForEvent(EVENT_VIRTUALCURSOR_CHANGED, matchContentDoc);
    await invokeContentTask(browser, [], () => {
      const { CommonUtils } = ChromeUtils.importESModule(
        "chrome://mochitests/content/browser/accessible/tests/browser/Common.sys.mjs"
      );
      let vc = CommonUtils.getAccessible(
        content.document,
        Ci.nsIAccessibleDocument
      ).virtualCursor;
      vc.position = CommonUtils.getAccessible(
        "input1",
        null,
        null,
        null,
        content.document
      );
    });
    vccEvent = (await onVCChanged).QueryInterface(
      nsIAccessibleVirtualCursorChangeEvent
    );
    isnot(vccEvent.oldAccessible, vccEvent.newAccessible, "positions differ");
    is(vccEvent.oldAccessible.id, "p1", "Old position is correct");
    is(vccEvent.newAccessible.id, "input1", "New position is correct");
    is(vccEvent.newStartOffset, -1, "New start offset is correct");
    is(vccEvent.newEndOffset, -1, "New end offset is correct");
    ok(!vccEvent.isFromUserInput, "not user initiated");
  },
  { iframe: true, remoteIframe: true }
);
