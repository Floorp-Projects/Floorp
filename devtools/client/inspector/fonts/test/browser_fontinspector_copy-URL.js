/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a context menu appears when right-clicking on a web font URL, and that the
// URL can be copied to the clipboard thanks to it.

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(async function() {
  let { view, toolbox } = await openFontInspectorForURL(TEST_URI);
  let viewDoc = view.document;
  let win = viewDoc.defaultView;

  let fontEl = getUsedFontsEls(viewDoc)[0];
  let linkEl = fontEl.querySelector(".font-url");

  // This is the container for all our popup menus in the toolbox. At this time, the
  // copy menu (xul <menupopup> element) does not exist inside it yet.
  let popupset = toolbox.doc.querySelector("popupset");

  // It is only created when the following event is received in the Font component.
  info("Right-clicking on the first font's link");
  EventUtils.synthesizeMouse(linkEl, 2, 2, {type: "contextmenu", button: 2}, win);

  // Now the menu exist and is in the process of getting shown. But since this happen
  // asynchronously, it is fine only starting to listen for the popupshown event only now.
  // Anyway, it isn't possible to start listening prior to this since the element does not
  // exist before.
  info("Waiting for the new menu to open");
  let menu = popupset.querySelector("menupopup[menu-api]");
  await once(menu, "popupshown");

  let items = menu.querySelectorAll("menuitem");
  is(items.length, 1, "There's only one menu item displayed");
  is(items[0].getAttribute("label"), "Copy Link", "This is the right menu item");

  info("Clicking the menu item and waiting for the clipboard to receive the URL");
  let expected = linkEl.href;
  await waitForClipboardPromise(() => items[0].click(), expected);

  info("Closing the menu");
  let onHidden = once(menu, "popuphidden");
  menu.hidePopup();
  await onHidden;
});
