/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

registerCleanupFunction(async function () {
  await task_resetState();
});

function changeSelection(listbox, down) {
  let selectPromise = BrowserTestUtils.waitForEvent(listbox, "select");
  EventUtils.synthesizeKey(down ? "VK_DOWN" : "VK_UP", {});
  return selectPromise;
}

add_task(async function test_downloads_keynav() {
  // Ensure that state is reset in case previous tests didn't finish.
  await task_resetState();

  await SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] });

  // Move the mouse pointer out of the way first so it doesn't
  // interfere with the selection.
  let listbox = document.getElementById("downloadsListBox");
  EventUtils.synthesizeMouse(listbox, -5, -5, { type: "mousemove" });

  let downloads = [];
  for (let i = 0; i < 2; i++) {
    downloads.push({ state: DownloadsCommon.DOWNLOAD_FINISHED });
  }
  downloads.push({ state: DownloadsCommon.DOWNLOAD_FAILED });
  downloads.push({ state: DownloadsCommon.DOWNLOAD_BLOCKED });

  await task_addDownloads(downloads);
  await task_openPanel();

  is(document.activeElement, listbox, "downloads list is focused");
  is(listbox.selectedIndex, 0, "downloads list selected index starts at 0");

  let footer = document.getElementById("downloadsHistory");

  await changeSelection(listbox, true);
  is(
    document.activeElement,
    listbox,
    "downloads list is focused after down to index 1"
  );
  is(
    listbox.selectedIndex,
    1,
    "downloads list selected index after down is pressed"
  );

  checkTabbing(listbox, 1);

  await changeSelection(listbox, true);
  is(
    document.activeElement,
    listbox,
    "downloads list is focused after down to index 2"
  );
  is(
    listbox.selectedIndex,
    2,
    "downloads list selected index after down to index 2"
  );

  checkTabbing(listbox, 2);

  await changeSelection(listbox, true);
  is(
    document.activeElement,
    listbox,
    "downloads list is focused after down to index 3"
  );
  is(
    listbox.selectedIndex,
    3,
    "downloads list selected index after down to index 3"
  );

  checkTabbing(listbox, 3);

  await changeSelection(listbox, true);
  is(document.activeElement, footer, "footer is focused");
  is(
    listbox.selectedIndex,
    -1,
    "downloads list selected index after down to footer"
  );

  EventUtils.synthesizeKey("VK_TAB", {});
  is(
    document.activeElement,
    listbox,
    "downloads list should be focused after tab when footer is focused"
  );
  is(
    listbox.selectedIndex,
    0,
    "downloads list should be focused after tab when footer is focused selected index"
  );

  // Move back to the footer.
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is(document.activeElement, footer, "downloads footer is focused again");
  is(
    listbox.selectedIndex,
    0,
    "downloads footer is focused again selected index"
  );

  EventUtils.synthesizeKey("VK_DOWN", {});
  is(
    document.activeElement,
    footer,
    "downloads footer is still focused after down past footer"
  );
  is(
    listbox.selectedIndex,
    -1,
    "downloads footer is still focused selected index after down past footer"
  );

  await changeSelection(listbox, false);
  is(
    document.activeElement,
    listbox,
    "downloads list is focused after up to index 3"
  );
  is(
    listbox.selectedIndex,
    3,
    "downloads list selected index after up to index 3"
  );

  await changeSelection(listbox, false);
  is(
    document.activeElement,
    listbox,
    "downloads list is focused after up to index 2"
  );
  is(
    listbox.selectedIndex,
    2,
    "downloads list selected index after up to index 2"
  );

  EventUtils.synthesizeMouseAtCenter(listbox.getItemAtIndex(0), {
    type: "mousemove",
  });
  EventUtils.synthesizeMouseAtCenter(listbox.getItemAtIndex(1), {
    type: "mousemove",
  });
  is(listbox.selectedIndex, 0, "downloads list selected index after mousemove");

  checkTabbing(listbox, 0);

  EventUtils.synthesizeKey("VK_UP", {});
  is(
    document.activeElement,
    listbox,
    "downloads list is still focused after up past start"
  );
  is(
    listbox.selectedIndex,
    0,
    "downloads list is still focused after up past start selected index"
  );

  // Move the mouse pointer out of the way again so we don't
  // hover over an item unintentionally if this test is run in verify mode.
  EventUtils.synthesizeMouse(listbox, -5, -5, { type: "mousemove" });

  await task_resetState();
});

async function checkTabbing(listbox, buttonIndex) {
  let button = listbox.getItemAtIndex(buttonIndex).querySelector("button");
  let footer = document.getElementById("downloadsHistory");

  listbox.clientWidth; // flush layout first

  EventUtils.synthesizeKey("VK_TAB", {});
  is(
    document.activeElement,
    button,
    "downloads button is focused after tab is pressed"
  );
  is(
    listbox.selectedIndex,
    buttonIndex,
    "downloads button selected index after tab is pressed"
  );

  EventUtils.synthesizeKey("VK_TAB", {});
  is(
    document.activeElement,
    footer,
    "downloads footer is focused after tab is pressed again"
  );
  is(
    listbox.selectedIndex,
    buttonIndex,
    "downloads footer selected index after tab is pressed again"
  );

  EventUtils.synthesizeKey("VK_TAB", {});
  is(
    document.activeElement,
    listbox,
    "downloads list is focused after tab is pressed yet again"
  );
  is(
    listbox.selectedIndex,
    buttonIndex,
    "downloads list selected index after tab is pressed yet again"
  );

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is(
    document.activeElement,
    footer,
    "downloads footer is focused after shift+tab is pressed"
  );
  is(
    listbox.selectedIndex,
    buttonIndex,
    "downloads footer selected index after shift+tab is pressed"
  );

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is(
    document.activeElement,
    button,
    "downloads button is focused after shift+tab is pressed again"
  );
  is(
    listbox.selectedIndex,
    buttonIndex,
    "downloads button selected index after shift+tab is pressed again"
  );

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is(
    document.activeElement,
    listbox,
    "downloads list is focused after shift+tab is pressed yet again"
  );
  is(
    listbox.selectedIndex,
    buttonIndex,
    "downloads list selected index after shift+tab is pressed yet again"
  );
}
