/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Menu API works

const URL = "data:text/html;charset=utf8,test page for menu api";
const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

add_task(async function() {
  info("Create a test tab and open the toolbox");
  const tab = await addTab(URL);
  const target = await TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "webconsole");

  // This test will involve localized strings, make sure the necessary FTL file is
  // available in the toolbox top window.
  toolbox.topWindow.MozXULElement.insertFTLIfNeeded("toolkit/main-window/editmenu.ftl");

  loadFTL(toolbox, "toolkit/main-window/editmenu.ftl");

  await testMenuItems();
  await testMenuPopup(toolbox);
  await testSubmenu(toolbox);
});

function testMenuItems() {
  const menu = new Menu();
  const menuItem1 = new MenuItem();
  const menuItem2 = new MenuItem();

  menu.append(menuItem1);
  menu.append(menuItem2);

  is(menu.items.length, 2, "Correct number of 'items'");
  is(menu.items[0], menuItem1, "Correct reference to MenuItem");
  is(menu.items[1], menuItem2, "Correct reference to MenuItem");
}

async function testMenuPopup(toolbox) {
  let clickFired = false;

  const menu = new Menu({
    id: "menu-popup",
  });
  menu.append(new MenuItem({ type: "separator" }));

  const MENU_ITEMS = [
    new MenuItem({
      id: "menu-item-1",
      label: "Normal Item",
      click: () => {
        info("Click callback has fired for menu item");
        clickFired = true;
      },
    }),
    new MenuItem({
      label: "Checked Item",
      type: "checkbox",
      checked: true,
    }),
    new MenuItem({
      label: "Radio Item",
      type: "radio",
    }),
    new MenuItem({
      label: "Disabled Item",
      disabled: true,
    }),
    new MenuItem({
      l10nID: "editmenu-undo",
    }),
  ];

  for (const item of MENU_ITEMS) {
    menu.append(item);
  }

  // Append an invisible MenuItem, which shouldn't show up in the DOM
  menu.append(new MenuItem({
    label: "Invisible",
    visible: false,
  }));

  menu.popup(0, 0, toolbox.doc);

  ok(toolbox.topDoc.querySelector("#menu-popup"), "A popup is in the DOM");

  const menuSeparators =
    toolbox.topDoc.querySelectorAll("#menu-popup > menuseparator");
  is(menuSeparators.length, 1, "A separator is in the menu");

  const menuItems = toolbox.topDoc.querySelectorAll("#menu-popup > menuitem");
  is(menuItems.length, MENU_ITEMS.length, "Correct number of menuitems");

  is(menuItems[0].id, MENU_ITEMS[0].id, "Correct id for menuitem");
  is(menuItems[0].getAttribute("label"), MENU_ITEMS[0].label, "Correct label");

  is(menuItems[1].getAttribute("label"), MENU_ITEMS[1].label, "Correct label");
  is(menuItems[1].getAttribute("type"), "checkbox", "Correct type attr");
  is(menuItems[1].getAttribute("checked"), "true", "Has checked attr");

  is(menuItems[2].getAttribute("label"), MENU_ITEMS[2].label, "Correct label");
  is(menuItems[2].getAttribute("type"), "radio", "Correct type attr");
  ok(!menuItems[2].hasAttribute("checked"), "Doesn't have checked attr");

  is(menuItems[3].getAttribute("label"), MENU_ITEMS[3].label, "Correct label");
  is(menuItems[3].getAttribute("disabled"), "true", "disabled attr menuitem");

  is(menuItems[4].getAttribute("data-l10n-id"), MENU_ITEMS[4].l10nID, "Correct localization attribute");

  await once(menu, "open");
  const closed = once(menu, "close");
  EventUtils.synthesizeMouseAtCenter(menuItems[0], {}, toolbox.topWindow);
  await closed;
  ok(clickFired, "Click has fired");

  ok(!toolbox.topDoc.querySelector("#menu-popup"), "Popup removed from the DOM");
}

async function testSubmenu(toolbox) {
  let clickFired = false;
  const menu = new Menu({
    id: "menu-popup",
  });
  const submenu = new Menu({
    id: "submenu-popup",
  });
  submenu.append(new MenuItem({
    label: "Submenu item",
    click: () => {
      info("Click callback has fired for submenu item");
      clickFired = true;
    },
  }));
  menu.append(new MenuItem({
    l10nID: "editmenu-copy",
    submenu: submenu,
  }));
  menu.append(new MenuItem({
    label: "Submenu parent with attributes",
    id: "submenu-parent-with-attrs",
    submenu: submenu,
    accesskey: "A",
    disabled: true,
  }));

  menu.popup(0, 0, toolbox.doc);
  ok(toolbox.topDoc.querySelector("#menu-popup"), "A popup is in the DOM");
  is(toolbox.topDoc.querySelectorAll("#menu-popup > menuitem").length, 0,
    "No menuitem children");

  const menus = toolbox.topDoc.querySelectorAll("#menu-popup > menu");
  is(menus.length, 2, "Correct number of menus");
  ok(!menus[0].hasAttribute("label"), "No label: should be set by localization");
  ok(!menus[0].hasAttribute("disabled"), "Correct disabled state");
  is(menus[0].getAttribute("data-l10n-id"), "editmenu-copy", "Correct localization attribute");

  is(menus[1].getAttribute("accesskey"), "A", "Correct accesskey");
  ok(menus[1].hasAttribute("disabled"), "Correct disabled state");
  is(menus[1].id, "submenu-parent-with-attrs", "Correct id");

  const subMenuItems = menus[0].querySelectorAll("menupopup > menuitem");
  is(subMenuItems.length, 1, "Correct number of submenu items");
  is(subMenuItems[0].getAttribute("label"), "Submenu item", "Correct label");

  await once(menu, "open");
  const closed = once(menu, "close");

  info("Using keyboard navigation to open, close, and reopen the submenu");
  let shown = once(menus[0], "popupshown");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await shown;

  const hidden = once(menus[0], "popuphidden");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  await hidden;

  shown = once(menus[0], "popupshown");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await shown;

  info("Clicking the submenu item");
  EventUtils.synthesizeMouseAtCenter(subMenuItems[0], {}, toolbox.topWindow);

  await closed;
  ok(clickFired, "Click has fired");
}
