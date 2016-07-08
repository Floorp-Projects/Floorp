/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Menu API works

const URL = "data:text/html;charset=utf8,test page for menu api";
const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

add_task(function* () {
  info("Create a test tab and open the toolbox");
  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "webconsole");

  yield testMenuItems();
  yield testMenuPopup(toolbox);
  yield testSubmenu(toolbox);
});

function* testMenuItems() {
  let menu = new Menu();
  let menuItem1 = new MenuItem();
  let menuItem2 = new MenuItem();

  menu.append(menuItem1);
  menu.append(menuItem2);

  is(menu.items.length, 2, "Correct number of 'items'");
  is(menu.items[0], menuItem1, "Correct reference to MenuItem");
  is(menu.items[1], menuItem2, "Correct reference to MenuItem");
}

function* testMenuPopup(toolbox) {
  let clickFired = false;

  let menu = new Menu({
    id: "menu-popup",
  });
  menu.append(new MenuItem({ type: "separator" }));

  let MENU_ITEMS = [
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
  ];

  for (let item of MENU_ITEMS) {
    menu.append(item);
  }

  // Append an invisible MenuItem, which shouldn't show up in the DOM
  menu.append(new MenuItem({
    label: "Invisible",
    visible: false,
  }));

  menu.popup(0, 0, toolbox);

  ok(toolbox.doc.querySelector("#menu-popup"), "A popup is in the DOM");

  let menuSeparators =
    toolbox.doc.querySelectorAll("#menu-popup > menuseparator");
  is(menuSeparators.length, 1, "A separator is in the menu");

  let menuItems = toolbox.doc.querySelectorAll("#menu-popup > menuitem");
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

  yield once(menu, "open");
  let closed = once(menu, "close");
  EventUtils.synthesizeMouseAtCenter(menuItems[0], {}, toolbox.win);
  yield closed;
  ok(clickFired, "Click has fired");

  ok(!toolbox.doc.querySelector("#menu-popup"), "Popup removed from the DOM");
}

function* testSubmenu(toolbox) {
  let clickFired = false;
  let menu = new Menu({
    id: "menu-popup",
  });
  let submenu = new Menu({
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
    label: "Submenu parent",
    submenu: submenu,
  }));
  menu.append(new MenuItem({
    label: "Submenu parent with attributes",
    id: "submenu-parent-with-attrs",
    submenu: submenu,
    accesskey: "A",
    disabled: true,
  }));

  menu.popup(0, 0, toolbox);
  ok(toolbox.doc.querySelector("#menu-popup"), "A popup is in the DOM");
  is(toolbox.doc.querySelectorAll("#menu-popup > menuitem").length, 0,
    "No menuitem children");

  let menus = toolbox.doc.querySelectorAll("#menu-popup > menu");
  is(menus.length, 2, "Correct number of menus");
  is(menus[0].getAttribute("label"), "Submenu parent", "Correct label");
  ok(!menus[0].hasAttribute("disabled"), "Correct disabled state");

  is(menus[1].getAttribute("accesskey"), "A", "Correct accesskey");
  ok(menus[1].hasAttribute("disabled"), "Correct disabled state");
  ok(menus[1].id, "submenu-parent-with-attrs", "Correct id");

  let subMenuItems = menus[0].querySelectorAll("menupopup > menuitem");
  is(subMenuItems.length, 1, "Correct number of submenu items");
  is(subMenuItems[0].getAttribute("label"), "Submenu item", "Correct label");

  yield once(menu, "open");
  let closed = once(menu, "close");

  info("Using keyboard navigation to open, close, and reopen the submenu");
  let shown = once(menus[0], "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {});
  EventUtils.synthesizeKey("VK_RIGHT", {});
  yield shown;

  let hidden = once(menus[0], "popuphidden");
  EventUtils.synthesizeKey("VK_LEFT", {});
  yield hidden;

  shown = once(menus[0], "popupshown");
  EventUtils.synthesizeKey("VK_RIGHT", {});
  yield shown;

  info("Clicking the submenu item");
  EventUtils.synthesizeMouseAtCenter(subMenuItems[0], {}, toolbox.win);

  yield closed;
  ok(clickFired, "Click has fired");
}
