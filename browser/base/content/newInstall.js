/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import(
  "resource://gre/modules/Services.jsm",
  {}
);

function init() {
  document.querySelector("button").addEventListener("command", () => {
    window.close();
  });

  if (navigator.platform == "MacIntel") {
    hideMenus();
    window.addEventListener("unload", showMenus);
  }
}

let gHidden = [];
let gCollapsed = [];

function hideItem(id) {
  let hiddenDoc = Services.appShell.hiddenDOMWindow.document;
  let element = hiddenDoc.getElementById(id);
  element.hidden = true;
  gHidden.push(element);
}

function collapseItem(id) {
  let hiddenDoc = Services.appShell.hiddenDOMWindow.document;
  let element = hiddenDoc.getElementById(id);
  element.collapsed = true;
  gCollapsed.push(element);
}

function hideMenus() {
  hideItem("macDockMenuNewWindow");
  hideItem("macDockMenuNewPrivateWindow");
  collapseItem("aboutName");
  collapseItem("menu_preferences");
  collapseItem("menu_mac_services");
}

function showMenus() {
  for (let menu of gHidden) {
    menu.hidden = false;
  }

  for (let menu of gCollapsed) {
    menu.collapsed = false;
  }
}
