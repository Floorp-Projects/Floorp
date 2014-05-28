/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let gSidebarMenu = document.getElementById("viewSidebarMenu");
let gTestSidebarItem = null;

let EVENTS = {
  click: 0, command: 0,
  onclick: 0, oncommand: 0
};

window.sawEvent = function(event, isattr) {
  let type = (isattr ? "on" : "") + event.type
  EVENTS[type]++;
};

registerCleanupFunction(() => {
  delete window.sawEvent;

  // Ensure sidebar is hidden after each test:
  if (!document.getElementById("sidebar-box").hidden) {
    toggleSidebar("", false);
  }
});

function checkExpectedEvents(expected) {
  for (let type of Object.keys(EVENTS)) {
    let count = (type in expected ? expected[type] : 0);
    is(EVENTS[type], count, "Should have seen the right number of " + type + " events");
    EVENTS[type] = 0;
  }
}

function createSidebarItem() {
  gTestSidebarItem = document.createElement("menuitem");
  gTestSidebarItem.id = "testsidebar";
  gTestSidebarItem.setAttribute("label", "Test Sidebar");
  gSidebarMenu.insertBefore(gTestSidebarItem, gSidebarMenu.firstChild);
}

function addWidget() {
  CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar");
  PanelUI.disableSingleSubviewPanelAnimations();
}

function removeWidget() {
  CustomizableUI.removeWidgetFromArea("sidebar-button");
  PanelUI.enableSingleSubviewPanelAnimations();
}

// Filters out the trailing menuseparators from the sidebar list
function getSidebarList() {
  let sidebars = [...gSidebarMenu.children];
  while (sidebars[sidebars.length - 1].localName == "menuseparator")
    sidebars.pop();
  return sidebars;
}

function compareElements(original, displayed) {
  let attrs = ["label", "key", "disabled", "hidden", "origin", "image", "checked"];
  for (let attr of attrs) {
    is(displayed.getAttribute(attr), original.getAttribute(attr), "Should have the same " + attr + " attribute");
  }
}

function compareList(original, displayed) {
  is(displayed.length, original.length, "Should have the same number of children");

  for (let i = 0; i < Math.min(original.length, displayed.length); i++) {
    compareElements(displayed[i], original[i]);
  }
}

let showSidebarPopup = Task.async(function*() {
  let button = document.getElementById("sidebar-button");
  let subview = document.getElementById("PanelUI-sidebar");

  let subviewShownPromise = subviewShown(subview);
  EventUtils.synthesizeMouseAtCenter(button, {});
  yield subviewShownPromise;
  return waitForCondition(() => !subview.panelMultiView.hasAttribute("transitioning"));
});

// Check the sidebar widget shows the default items
add_task(function*() {
  addWidget();

  yield showSidebarPopup();

  let sidebars = getSidebarList();
  let displayed = [...document.getElementById("PanelUI-sidebarItems").children];
  compareList(sidebars, displayed);

  let subview = document.getElementById("PanelUI-sidebar");
  let subviewHiddenPromise = subviewHidden(subview);
  document.getElementById("customizationui-widget-panel").hidePopup();
  yield subviewHiddenPromise;

  removeWidget();
});

function add_sidebar_task(description, setup, teardown) {
  add_task(function*() {
    info(description);
    createSidebarItem();
    addWidget();
    yield setup();

    CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar");

    yield showSidebarPopup();

    let sidebars = getSidebarList();
    let displayed = [...document.getElementById("PanelUI-sidebarItems").children];
    compareList(sidebars, displayed);

    is(displayed[0].label, "Test Sidebar", "Should have the right element at the top");
    let subview = document.getElementById("PanelUI-sidebar");
    let subviewHiddenPromise = subviewHidden(subview);
    EventUtils.synthesizeMouseAtCenter(displayed[0], {});
    yield subviewHiddenPromise;

    yield teardown();
    gTestSidebarItem.remove();
    removeWidget();
  });
}

add_sidebar_task(
  "Check that a sidebar that uses a command event listener works",
function*() {
  gTestSidebarItem.addEventListener("command", sawEvent);
}, function*() {
  checkExpectedEvents({ command: 1 });
});

add_sidebar_task(
  "Check that a sidebar that uses a click event listener works",
function*() {
  gTestSidebarItem.addEventListener("click", sawEvent);
}, function*() {
  checkExpectedEvents({ click: 1 });
});

add_sidebar_task(
  "Check that a sidebar that uses both click and command event listeners works",
function*() {
  gTestSidebarItem.addEventListener("command", sawEvent);
  gTestSidebarItem.addEventListener("click", sawEvent);
}, function*() {
  checkExpectedEvents({ command: 1, click: 1 });
});

add_sidebar_task(
  "Check that a sidebar that uses an oncommand attribute works",
function*() {
  gTestSidebarItem.setAttribute("oncommand", "sawEvent(event, true)");
}, function*() {
  checkExpectedEvents({ oncommand: 1 });
});

add_sidebar_task(
  "Check that a sidebar that uses an onclick attribute works",
function*() {
  gTestSidebarItem.setAttribute("onclick", "sawEvent(event, true)");
}, function*() {
  checkExpectedEvents({ onclick: 1 });
});

add_sidebar_task(
  "Check that a sidebar that uses both onclick and oncommand attributes works",
function*() {
  gTestSidebarItem.setAttribute("onclick", "sawEvent(event, true)");
  gTestSidebarItem.setAttribute("oncommand", "sawEvent(event, true)");
}, function*() {
  checkExpectedEvents({ onclick: 1, oncommand: 1 });
});

add_sidebar_task(
  "Check that a sidebar that uses an onclick attribute and a command listener works",
function*() {
  gTestSidebarItem.setAttribute("onclick", "sawEvent(event, true)");
  gTestSidebarItem.addEventListener("command", sawEvent);
}, function*() {
  checkExpectedEvents({ onclick: 1, command: 1 });
});

add_sidebar_task(
  "Check that a sidebar that uses an oncommand attribute and a click listener works",
function*() {
  gTestSidebarItem.setAttribute("oncommand", "sawEvent(event, true)");
  gTestSidebarItem.addEventListener("click", sawEvent);
}, function*() {
  checkExpectedEvents({ click: 1, oncommand: 1 });
});

add_sidebar_task(
  "A sidebar with both onclick attribute and click listener sees only one event :(",
function*() {
  gTestSidebarItem.setAttribute("onclick", "sawEvent(event, true)");
  gTestSidebarItem.addEventListener("click", sawEvent);
}, function*() {
  checkExpectedEvents({ onclick: 1 });
});

add_sidebar_task(
  "A sidebar with both oncommand attribute and command listener sees only one event :(",
function*() {
  gTestSidebarItem.setAttribute("oncommand", "sawEvent(event, true)");
  gTestSidebarItem.addEventListener("command", sawEvent);
}, function*() {
  checkExpectedEvents({ oncommand: 1 });
});

add_sidebar_task(
  "Check that a sidebar that uses a broadcaster with an oncommand attribute works",
function*() {
  let broadcaster = document.createElement("broadcaster");
  broadcaster.setAttribute("id", "testbroadcaster");
  broadcaster.setAttribute("oncommand", "sawEvent(event, true)");
  broadcaster.setAttribute("label", "Test Sidebar");
  document.getElementById("mainBroadcasterSet").appendChild(broadcaster);

  gTestSidebarItem.setAttribute("observes", "testbroadcaster");
}, function*() {
  checkExpectedEvents({ oncommand: 1 });
  document.getElementById("testbroadcaster").remove();
});

add_sidebar_task(
  "Check that a sidebar that uses a broadcaster with an onclick attribute works",
function*() {
  let broadcaster = document.createElement("broadcaster");
  broadcaster.setAttribute("id", "testbroadcaster");
  broadcaster.setAttribute("onclick", "sawEvent(event, true)");
  broadcaster.setAttribute("label", "Test Sidebar");
  document.getElementById("mainBroadcasterSet").appendChild(broadcaster);

  gTestSidebarItem.setAttribute("observes", "testbroadcaster");
}, function*() {
  checkExpectedEvents({ onclick: 1 });
  document.getElementById("testbroadcaster").remove();
});

add_sidebar_task(
  "Check that a sidebar that uses a broadcaster with both onclick and oncommand attributes works",
function*() {
  let broadcaster = document.createElement("broadcaster");
  broadcaster.setAttribute("id", "testbroadcaster");
  broadcaster.setAttribute("onclick", "sawEvent(event, true)");
  broadcaster.setAttribute("oncommand", "sawEvent(event, true)");
  broadcaster.setAttribute("label", "Test Sidebar");
  document.getElementById("mainBroadcasterSet").appendChild(broadcaster);

  gTestSidebarItem.setAttribute("observes", "testbroadcaster");
}, function*() {
  checkExpectedEvents({ onclick: 1, oncommand: 1 });
  document.getElementById("testbroadcaster").remove();
});

add_sidebar_task(
  "Check that a sidebar with a click listener and a broadcaster with an oncommand attribute works",
function*() {
  let broadcaster = document.createElement("broadcaster");
  broadcaster.setAttribute("id", "testbroadcaster");
  broadcaster.setAttribute("oncommand", "sawEvent(event, true)");
  broadcaster.setAttribute("label", "Test Sidebar");
  document.getElementById("mainBroadcasterSet").appendChild(broadcaster);

  gTestSidebarItem.setAttribute("observes", "testbroadcaster");
  gTestSidebarItem.addEventListener("click", sawEvent);
}, function*() {
  checkExpectedEvents({ click: 1, oncommand: 1 });
  document.getElementById("testbroadcaster").remove();
});

add_sidebar_task(
  "Check that a sidebar with a command listener and a broadcaster with an onclick attribute works",
function*() {
  let broadcaster = document.createElement("broadcaster");
  broadcaster.setAttribute("id", "testbroadcaster");
  broadcaster.setAttribute("onclick", "sawEvent(event, true)");
  broadcaster.setAttribute("label", "Test Sidebar");
  document.getElementById("mainBroadcasterSet").appendChild(broadcaster);

  gTestSidebarItem.setAttribute("observes", "testbroadcaster");
  gTestSidebarItem.addEventListener("command", sawEvent);
}, function*() {
  checkExpectedEvents({ onclick: 1, command: 1 });
  document.getElementById("testbroadcaster").remove();
});

add_sidebar_task(
  "Check that a sidebar with a click listener and a broadcaster with an onclick " +
  "attribute only sees one event :(",
function*() {
  let broadcaster = document.createElement("broadcaster");
  broadcaster.setAttribute("id", "testbroadcaster");
  broadcaster.setAttribute("onclick", "sawEvent(event, true)");
  broadcaster.setAttribute("label", "Test Sidebar");
  document.getElementById("mainBroadcasterSet").appendChild(broadcaster);

  gTestSidebarItem.setAttribute("observes", "testbroadcaster");
  gTestSidebarItem.addEventListener("click", sawEvent);
}, function*() {
  checkExpectedEvents({ onclick: 1 });
  document.getElementById("testbroadcaster").remove();
});

add_sidebar_task(
  "Check that a sidebar with a command listener and a broadcaster with an oncommand " +
  "attribute only sees one event :(",
function*() {
  let broadcaster = document.createElement("broadcaster");
  broadcaster.setAttribute("id", "testbroadcaster");
  broadcaster.setAttribute("oncommand", "sawEvent(event, true)");
  broadcaster.setAttribute("label", "Test Sidebar");
  document.getElementById("mainBroadcasterSet").appendChild(broadcaster);

  gTestSidebarItem.setAttribute("observes", "testbroadcaster");
  gTestSidebarItem.addEventListener("command", sawEvent);
}, function*() {
  checkExpectedEvents({ oncommand: 1 });
  document.getElementById("testbroadcaster").remove();
});

add_sidebar_task(
  "Check that a sidebar that uses a command element with a command event listener works",
function*() {
  let command = document.createElement("command");
  command.setAttribute("id", "testcommand");
  document.getElementById("mainCommandSet").appendChild(command);
  command.addEventListener("command", sawEvent);

  gTestSidebarItem.setAttribute("command", "testcommand");
}, function*() {
  checkExpectedEvents({ command: 1 });
  document.getElementById("testcommand").remove();
});

add_sidebar_task(
  "Check that a sidebar that uses a command element with an oncommand attribute works",
function*() {
  let command = document.createElement("command");
  command.setAttribute("id", "testcommand");
  command.setAttribute("oncommand", "sawEvent(event, true)");
  document.getElementById("mainCommandSet").appendChild(command);

  gTestSidebarItem.setAttribute("command", "testcommand");
}, function*() {
  checkExpectedEvents({ oncommand: 1 });
  document.getElementById("testcommand").remove();
});

add_sidebar_task("Check that a sidebar that uses a command element with a " +
  "command event listener and oncommand attribute works",
function*() {
  let command = document.createElement("command");
  command.setAttribute("id", "testcommand");
  command.setAttribute("oncommand", "sawEvent(event, true)");
  document.getElementById("mainCommandSet").appendChild(command);
  command.addEventListener("command", sawEvent);

  gTestSidebarItem.setAttribute("command", "testcommand");
}, function*() {
  checkExpectedEvents({ command: 1, oncommand: 1 });
  document.getElementById("testcommand").remove();
});

add_sidebar_task(
  "A sidebar with a command element will still see click events",
function*() {
  let command = document.createElement("command");
  command.setAttribute("id", "testcommand");
  command.setAttribute("oncommand", "sawEvent(event, true)");
  document.getElementById("mainCommandSet").appendChild(command);
  command.addEventListener("command", sawEvent);

  gTestSidebarItem.setAttribute("command", "testcommand");
  gTestSidebarItem.addEventListener("click", sawEvent);
}, function*() {
  checkExpectedEvents({ click: 1, command: 1, oncommand: 1 });
  document.getElementById("testcommand").remove();
});
