/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Promise.jsm");

const isOSX = (Services.appinfo.OS === "Darwin");

let gTests = [
  {
    desc: "Right-click on the home button should show a context menu with options to move it.",
    setup: null,
    run: function() {
      let contextMenu = document.getElementById("toolbar-context-menu");
      let shownPromise = contextMenuShown(contextMenu);
      let homeButton = document.getElementById("home-button");
      EventUtils.synthesizeMouse(homeButton, 2, 2, {type: "contextmenu", button: 2 });
      yield shownPromise;

      let expectedEntries = [
        [".customize-context-addToPanel", true],
        [".customize-context-removeFromToolbar", true],
        ["---"]
      ];
      if (!isOSX) {
        expectedEntries.push(["#toggle_toolbar-menubar", true]);
      }
      expectedEntries.push(
        ["#toggle_PersonalToolbar", true],
        ["---"],
        [".viewCustomizeToolbar", true]
      );
      checkContextMenu(contextMenu, expectedEntries);

      let hiddenPromise = contextMenuHidden(contextMenu);
      contextMenu.hidePopup();
      yield hiddenPromise;
    },
    teardown: null
  },
  {
    desc: "Right-click on the urlbar-container should show a context menu with disabled options to move it.",
    setup: null,
    run: function() {
      let contextMenu = document.getElementById("toolbar-context-menu");
      let shownPromise = contextMenuShown(contextMenu);
      let urlBarContainer = document.getElementById("urlbar-container");
      // Need to make sure not to click within an edit field.
      let urlbarRect = urlBarContainer.getBoundingClientRect();
      EventUtils.synthesizeMouse(urlBarContainer, 100, urlbarRect.height - 1, {type: "contextmenu", button: 2 });
      yield shownPromise;

      let expectedEntries = [
        [".customize-context-addToPanel", false],
        [".customize-context-removeFromToolbar", false],
        ["---"]
      ];
      if (!isOSX) {
        expectedEntries.push(["#toggle_toolbar-menubar", true]);
      }
      expectedEntries.push(
        ["#toggle_PersonalToolbar", true],
        ["---"],
        [".viewCustomizeToolbar", true]
      );
      checkContextMenu(contextMenu, expectedEntries);

      let hiddenPromise = contextMenuHidden(contextMenu);
      contextMenu.hidePopup();
      yield hiddenPromise;
    },
    teardown: null
  },
  {
    desc: "Right-click on an item within the menu panel should show a context menu with options to move it.",
    setup: null,
    run: function() {
      let shownPanelPromise = promisePanelShown(window);
      PanelUI.toggle({type: "command"});
      yield shownPanelPromise;

      let contextMenu = document.getElementById("customizationPanelItemContextMenu");
      let shownContextPromise = contextMenuShown(contextMenu);
      let newWindowButton = document.getElementById("new-window-button");
      ok(newWindowButton, "new-window-button was found");
      EventUtils.synthesizeMouse(newWindowButton, 2, 2, {type: "contextmenu", button: 2});
      yield shownContextPromise;

      is(PanelUI.panel.state, "open", "The PanelUI should still be open.");

      let expectedEntries = [
        [".customize-context-addToToolbar", true],
        [".customize-context-removeFromPanel", true],
        ["---"],
        [".viewCustomizeToolbar", true]
      ];
      checkContextMenu(contextMenu, expectedEntries);

      let hiddenContextPromise = contextMenuHidden(contextMenu);
      contextMenu.hidePopup();
      yield hiddenContextPromise;

      let hiddenPromise = promisePanelHidden(window);
      PanelUI.toggle({type: "command"});
      yield hiddenPromise;
    },
    teardown: null
  },
  {
    desc: "Right-click on the home button while in customization mode should show a context menu with options to move it.",
    setup: startCustomizing,
    run: function () {
      let contextMenu = document.getElementById("toolbar-context-menu");
      let shownPromise = contextMenuShown(contextMenu);
      let homeButton = document.getElementById("wrapper-home-button");
      EventUtils.synthesizeMouse(homeButton, 2, 2, {type: "contextmenu", button: 2});
      yield shownPromise;

      let expectedEntries = [
        [".customize-context-addToPanel", true],
        [".customize-context-removeFromToolbar", true],
        ["---"]
      ];
      if (!isOSX) {
        expectedEntries.push(["#toggle_toolbar-menubar", true]);
      }
      expectedEntries.push(
        ["#toggle_PersonalToolbar", true],
        ["---"],
        [".viewCustomizeToolbar", false]
      );
      checkContextMenu(contextMenu, expectedEntries);

      let hiddenContextPromise = contextMenuHidden(contextMenu);
      contextMenu.hidePopup();
      yield hiddenContextPromise;
    },
    teardown: null
  },
  {
    desc: "Right-click on an item in the palette should show a context menu with options to move it.",
    setup: null,
    run: function () {
      let contextMenu = document.getElementById("customizationPaletteItemContextMenu");
      let shownPromise = contextMenuShown(contextMenu);
      let openFileButton = document.getElementById("wrapper-open-file-button");
      EventUtils.synthesizeMouse(openFileButton, 2, 2, {type: "contextmenu", button: 2});
      yield shownPromise;

      let expectedEntries = [
        [".customize-context-addToToolbar", true],
        [".customize-context-addToPanel", true],
      ];
      checkContextMenu(contextMenu, expectedEntries);

      let hiddenContextPromise = contextMenuHidden(contextMenu);
      contextMenu.hidePopup();
      yield hiddenContextPromise;
    },
    teardown: null
  },
  {
    desc: "Right-click on an item in the panel while in customization mode should show a context menu with options to move it.",
    setup: null,
    run: function () {
      let contextMenu = document.getElementById("customizationPanelItemContextMenu");
      let shownPromise = contextMenuShown(contextMenu);
      let newWindowButton = document.getElementById("wrapper-new-window-button");
      EventUtils.synthesizeMouse(newWindowButton, 2, 2, {type: "contextmenu", button: 2});
      yield shownPromise;

      let expectedEntries = [
        [".customize-context-addToToolbar", true],
        [".customize-context-removeFromPanel", true],
        ["---"],
        [".viewCustomizeToolbar", false]
      ];
      checkContextMenu(contextMenu, expectedEntries);

      let hiddenContextPromise = contextMenuHidden(contextMenu);
      contextMenu.hidePopup();
      yield hiddenContextPromise;
    },
    teardown: endCustomizing
  },
];

function test() {
  waitForExplicitFinish();
  runTests(gTests);
}

function contextMenuShown(aContextMenu) {
  let deferred = Promise.defer();
  function onPopupShown(e) {
    aContextMenu.removeEventListener("popupshown", onPopupShown);
    deferred.resolve();
  };
  aContextMenu.addEventListener("popupshown", onPopupShown);
  return deferred.promise;
}

function contextMenuHidden(aContextMenu) {
  let deferred = Promise.defer();
  function onPopupHidden(e) {
    aContextMenu.removeEventListener("popuphidden", onPopupHidden);
    deferred.resolve();
  };
  aContextMenu.addEventListener("popuphidden", onPopupHidden);
  return deferred.promise;
}

// This is a simpler version of the context menu check that
// exists in contextmenu_common.js.
function checkContextMenu(aContextMenu, aExpectedEntries) {
  let childNodes = aContextMenu.childNodes;
  for (let i = 0; i < childNodes.length; i++) {
    let menuitem = childNodes[i];
    try {
      if (aExpectedEntries[i][0] == "---") {
        is(menuitem.localName, "menuseparator", "menuseparator expected");
        continue;
      }

      let selector = aExpectedEntries[i][0];
      ok(menuitem.mozMatchesSelector(selector), "menuitem should match " + selector + " selector");
      let commandValue = menuitem.getAttribute("command");
      let relatedCommand = commandValue ? document.getElementById(commandValue) : null;
      let menuItemDisabled = relatedCommand ?
                               relatedCommand.getAttribute("disabled") == "true" :
                               menuitem.getAttribute("disabled") == "true";
      is(menuItemDisabled, !aExpectedEntries[i][1], "disabled state wrong for " + selector);
    } catch (e) {
      ok(false, "Exception when checking context menu: " + e);
    }
  }
}
