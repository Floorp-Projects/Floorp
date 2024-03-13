/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `<input id="file" type="file">`,
  async function (browser, docAcc) {
    info("Focusing file input");
    await runPython(`
      global focused
      focused = WaitForWinEvent(EVENT_OBJECT_FOCUS, "file")
    `);
    const file = findAccessibleChildByID(docAcc, "file");
    file.takeFocus();
    await runPython(`
      focused.wait()
    `);
    ok(true, "file input got focus");
    info("Opening file picker");
    await runPython(`
      global focused
      focused = WaitForWinEvent(
        EVENT_OBJECT_FOCUS,
        lambda evt: getWindowClass(evt.hwnd) == "Edit"
      )
    `);
    file.doAction(0);
    await runPython(`
      global event
      event = focused.wait()
    `);
    ok(true, "Picker got focus");
    info("Dismissing picker");
    await runPython(`
      # If the picker is dismissed too quickly, it seems to re-enable the root
      # window before we do. This sleep isn't ideal, but it's more likely to
      # reproduce the case that our root window gets focus before it is enabled.
      # See bug 1883568 for further details.
      import time
      time.sleep(1)
      focused = WaitForWinEvent(EVENT_OBJECT_FOCUS, "file")
      # Sending key presses to the picker is unreliable, so use WM_CLOSE.
      pickerRoot = user32.GetAncestor(event.hwnd, GA_ROOT)
      user32.SendMessageW(pickerRoot, WM_CLOSE, 0, 0)
      focused.wait()
    `);
    ok(true, "file input got focus");
  }
);
