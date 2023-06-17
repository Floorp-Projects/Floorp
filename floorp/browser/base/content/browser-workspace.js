/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//use _gBrowser instead of gBrowser when startup
const WORKSPACE_TAB_ENABLED_PREF = "floorp.browser.workspace.tab.enabled";
const WORKSPACE_CURRENT_PREF = "floorp.browser.workspace.current";
const WORKSPACE_ALL_PREF = "floorp.browser.workspace.all";
const WORKSPACE_TABS_PREF = "floorp.browser.workspace.tabs.state";
const l10n = new Localization(["browser/floorp.ftl"], true);
const defaultWorkspaceName = Services.prefs.getStringPref(
  WORKSPACE_ALL_PREF
).split(",")[0];

const workspaceFunctions = {
  eventListeners: {
    tabAddEventListeners: {
      handleTabOpen() {
        let tabs = gBrowser.tabs;
        let lastTab = null;
        let firstTab = null;
        document
          .querySelector(`[floorp-lastVisibleTab]`)
          ?.removeAttribute("floorp-lastVisibleTab");
        document
          .querySelector(`[floorp-firstVisibleTab]`)
          ?.removeAttribute("floorp-firstVisibleTab");
        for (let i = 0; i < tabs.length; i++) {
          let tab = tabs[i];
          if (!tab.hasAttribute("floorp-workspace")) {
            tab.setAttribute(
              "floorp-workspace",
              Services.prefs.getStringPref(WORKSPACE_CURRENT_PREF)
            );
          }
          if (
            tab.getAttribute("floorp-workspace") ==
            Services.prefs.getStringPref(WORKSPACE_CURRENT_PREF)
          ) {
            lastTab = tab;
          }
          if (
            tab.getAttribute("floorp-workspace") ==
              Services.prefs.getStringPref(WORKSPACE_CURRENT_PREF) &&
            firstTab == null
          ) {
            tab.setAttribute("floorp-firstVisibleTab", "true");
            firstTab = tab;
          }
        }
        lastTab?.setAttribute("floorp-lastVisibleTab", "true");
        workspaceFunctions.manageWorkspaceFunctions.saveWorkspaceState();
      },

      handleTabClose() {
        window.setTimeout(() => {
          document
            .querySelector(`[floorp-firstVisibleTab]`)
            ?.removeAttribute("floorp-firstVisibleTab");
          document
            .querySelector(`[floorp-lastVisibleTab]`)
            ?.removeAttribute("floorp-lastVisibleTab");
          document
            .querySelector(`tab:not([hidden])`)
            .setAttribute("floorp-firstVisibleTab", "true");
          let elems = document.querySelectorAll(`tab:not([hidden])`);
          elems[elems.length - 1].setAttribute("floorp-lastVisibleTab", "true");
          workspaceFunctions.manageWorkspaceFunctions.saveWorkspaceState();
        }, 400);
      },

      handleTabSelect() {
        workspaceFunctions.manageWorkspaceFunctions.setCurrentWorkspace();
        workspaceFunctions.tabFunctions.addLastShowedWorkspaceTab();
      },

      handleTabMove() {
        workspaceFunctions.manageWorkspaceFunctions.saveWorkspaceState();
        document
          .querySelector(`[floorp-firstVisibleTab]`)
          ?.removeAttribute("floorp-firstVisibleTab");
        document
          .querySelector(`[floorp-lastVisibleTab]`)
          ?.removeAttribute("floorp-lastVisibleTab");
        document
          .querySelector(`tab:not([hidden])`)
          .setAttribute("floorp-firstVisibleTab", "true");
        let elems = document.querySelectorAll(`tab:not([hidden])`);
        elems[elems.length - 1].setAttribute("floorp-lastVisibleTab", "true");
      },
    },

    prefsEventListeners: {
      handleWorkspacePrefChange() {
        workspaceFunctions.manageWorkspaceFunctions.setCurrentWorkspace();
        workspaceFunctions.WorkspaceContextMenu.setMenuItemCheckCSS();
      },

      handleWorkspaceTabPrefChange() {
        document.querySelector(
          "#workspace-button"
        ).style.display = Services.prefs.getBoolPref(WORKSPACE_TAB_ENABLED_PREF)
          ? ""
          : "none";
        if (!Services.prefs.getBoolPref(WORKSPACE_TAB_ENABLED_PREF)) {
          document
            .querySelector(`[floorp-firstVisibleTab]`)
            ?.removeAttribute("floorp-firstVisibleTab");
          document
            .querySelector(`[floorp-lastVisibleTab]`)
            ?.removeAttribute("floorp-lastVisibleTab");
        }
        workspaceFunctions.manageWorkspaceFunctions.setCurrentWorkspace();
      },
    },

    keyboradEventListeners: {
      handle_keydown(event) {
        if (event.shiftKey && event.key === "ArrowUp") {
          workspaceFunctions.manageWorkspaceFunctions.changeWorkspaceToBeforeNext();
        } else if (event.shiftKey && event.key === "ArrowDown") {
          workspaceFunctions.manageWorkspaceFunctions.changeWorkspaceToAfterNext();
        }
      },
    },
  },
  manageWorkspaceFunctions: {
    initWorkspace() {
      // First run
      if (!Services.prefs.prefHasUserValue(WORKSPACE_CURRENT_PREF)) {
        Services.prefs.setStringPref(
          WORKSPACE_CURRENT_PREF,
          l10n.formatValueSync("workspace-default")
        );
        Services.prefs.setStringPref(WORKSPACE_ALL_PREF, l10n.formatValueSync("workspace-default"));
      }

      let tabs = gBrowser.tabs;

      if (Services.prefs.getStringPref(WORKSPACE_TABS_PREF) === "[]") {
        for (let i = 0; i < tabs.length; i++) {
          tabs[i].setAttribute(
            "floorp-workspace",
            Services.prefs.getStringPref(WORKSPACE_CURRENT_PREF)
          );
        }
      } else {
        let tabsState = JSON.parse(
          Services.prefs.getStringPref(WORKSPACE_TABS_PREF)
        );
        for (let i = 0; i < tabs.length; i++) {
          let tabStateSetting = tabsState[i];
          let workspace =
            tabStateSetting?.[i]?.workspace ??
            Services.prefs.getStringPref(WORKSPACE_CURRENT_PREF);
          tabs[i].setAttribute("floorp-workspace", workspace);
        }
      }

      const toolbarButtonEle = window.MozXULElement.parseXULToFragment(`
        <toolbarbutton id="workspace-button"
                        class="toolbarbutton-1 chromeclass-toolbar-additional"
                        label="Workspace"
                        tooltiptext="Workspace"
                        type="menu"
                        style="list-style-image: url('chrome://browser/skin/workspace-floorp.png');">
          <menupopup id="workspace-menu" context="workspace-menu-context">
            <toolbarbutton style="list-style-image: url('chrome://global/skin/icons/plus.svg');"
                    id="addNewWorkspaceButton"        data-l10n-id="workspace-add" class="subviewbutton subviewbutton-nav" oncommand="workspaceFunctions.manageWorkspaceFunctions.addNewWorkspace();"/>
          </menupopup>
        </toolbarbutton>
      `);

      document.querySelector(".toolbar-items").before(toolbarButtonEle);
      if (!Services.prefs.getBoolPref(WORKSPACE_TAB_ENABLED_PREF)) {
        document.querySelector("#workspace-button").style.display = "none";
      }

      // Add workspace menu from preference
      let workspaceAll = Services.prefs
        .getStringPref(WORKSPACE_ALL_PREF)
        .split(",");
      for (let i = 0; i < workspaceAll.length; i++) {
        let label = workspaceAll[i];
        workspaceFunctions.WorkspaceContextMenu.addWorkspaceElemToMenu(label);
      }

      // Add attribute to tab
      workspaceFunctions.tabFunctions.addLastShowedWorkspaceTab();

      //add keybooard shortcut
      let keyElem = document.createElement("key");
      keyElem.setAttribute("id", "floorp-workspace-key");
      keyElem.setAttribute("keycode", "VK_F9");
      keyElem.setAttribute("oncommand", "restartbrowser();");
      document.getElementById("mainKeyset").appendChild(keyElem);

      workspaceFunctions.TabContextMenu.addContextMenuToTabContext();
      workspaceFunctions.manageWorkspaceFunctions.saveWorkspaceState();
      workspaceFunctions.WorkspaceContextMenu.setMenuItemCheckCSS();
    },

    addNewWorkspace() {
      let allWorkspace = Services.prefs
        .getStringPref(WORKSPACE_ALL_PREF)
        .split(",");
      let l10n = new Localization(["browser/floorp.ftl"], true);
      let prompts = Services.prompt;
      let check = { value: false };
      let pattern = /^[\p{L}\p{N}]+$/u;
      let input = { value: "" };
      let result = prompts.prompt(
        null,
        l10n.formatValueSync("workspace-prompt-title"),
        l10n.formatValueSync("please-enter-workspace-name") +
          "\n" +
          l10n.formatValueSync("please-enter-workspace-name-2"),
        input,
        null,
        check
      );

      if (
        result &&
        allWorkspace.includes(input.value) == -1 &&
        input.value != "" &&
        input.value.length < 20 &&
        input.value != l10n.formatValueSync("workspace-default") &&
        pattern.test(input.value)
      ) {
        let label = input.value;
        let workspaceAll = Services.prefs
          .getStringPref(WORKSPACE_ALL_PREF)
          .split(",");
        try {
          workspaceFunctions.WorkspaceContextMenu.addWorkspaceElemToMenu(label);
        } catch (e) {
          prompts.alert(
            null,
            l10n.formatValueSync("workspace-prompt-title"),
            l10n.formatValueSync("workspace-error") +
              "\n" +
              l10n.formatValueSync("workspace-error-discription")
          );
        }
        workspaceAll.push(label);
        Services.prefs.setStringPref(WORKSPACE_ALL_PREF, workspaceAll);
      } else if (result === false) {
        /* empty */
      } else {
        prompts.alert(
          null,
          l10n.formatValueSync("workspace-prompt-title"),
          l10n.formatValueSync("workspace-error") +
            "\n" +
            l10n.formatValueSync("workspace-error-discription")
        );
      }
    },

    deleteworkspace(workspace) {
      if (workspace !== defaultWorkspaceName) {
        let allWorkspaces = Services.prefs
          .getCharPref(WORKSPACE_ALL_PREF)
          .split(",");
        let index = allWorkspaces.indexOf(workspace);
        let currentWorkspace = Services.prefs.getStringPref(
          WORKSPACE_CURRENT_PREF
        );

        //move to other workspace
        if (currentWorkspace == workspace) {
          workspaceFunctions.manageWorkspaceFunctions.changeWorkspace(
            allWorkspaces[0]
          );
          Services.prefs.setStringPref(
            WORKSPACE_CURRENT_PREF,
            defaultWorkspaceName
          );
          workspaceFunctions.manageWorkspaceFunctions.setCurrentWorkspace();
        }
        allWorkspaces.splice(index, 1);
        Services.prefs.setCharPref(WORKSPACE_ALL_PREF, allWorkspaces.join(","));

        //delete workspace tabs
        let tabs = gBrowser.tabs;
        for (let i = 0; i < tabs.length; i++) {
          let tab = tabs[i];
          let tabWorkspace = tab.getAttribute("floorp-workspace");
          if (tabWorkspace == workspace) {
            gBrowser.removeTab(tab);
          }
        }
        //delete workspace menuitem
        let menuitem = document.getElementById(`workspace-box-${workspace}`);
        menuitem.remove();
      }
    },

    renameWorkspace(label) {
      label = label.replace(/\s+/g, "-");
      if (label == defaultWorkspaceName) {
        return;
      }
      let prompts = Services.prompt;
      let l10n = new Localization(["browser/floorp.ftl"], true);
      let check = { value: false };
      let pattern = /^[\p{L}\p{N}\s]+$/u;
      let input = { value: "" };
      let result = prompts.prompt(
        null,
        l10n.formatValueSync("workspace-prompt-title"),
        l10n.formatValueSync("please-enter-workspace-name") +
          "\n" +
          l10n.formatValueSync("please-enter-workspace-name-2"),
        input,
        null,
        check
      );

      if (
        result &&
        input.value != "" &&
        input.value.length < 20 &&
        input.value != l10n.formatValueSync("workspace-default") &&
        pattern.test(input.value)
      ) {

        input.value = input.value.replace(/\s+/g, "-");
        let workspaceAll = Services.prefs
          .getStringPref(WORKSPACE_ALL_PREF)
          .split(",");
        let index = workspaceAll.indexOf(label);
        workspaceAll[index] = input.value;
        Services.prefs.setStringPref(WORKSPACE_ALL_PREF, workspaceAll);

        //Tabs
        let tabs = gBrowser.tabs;
        for (let i = 0; i < tabs.length; i++) {
          let tab = tabs[i];
          if (tab.getAttribute("floorp-workspace") == label) {
            tab.setAttribute("floorp-workspace", input.value);
          }
        }
        workspaceFunctions.manageWorkspaceFunctions.saveWorkspaceState();

        //lastShowWorkspaceTab
        document
          .querySelector(`[lastShowWorkspaceTab-${label}]`)
          ?.setAttribute(`lastShowWorkspaceTab-${input.value}`, "true");

        //menuitem
        let menuitem = document.querySelector(`#workspace-box-${label}`);
        let nextAfterMenuitem = menuitem.nextSibling;

        menuitem.remove();
        workspaceFunctions.WorkspaceContextMenu.addWorkspaceElemToMenu(
          input.value,
          nextAfterMenuitem
        );

        let currentWorkspace = Services.prefs.getStringPref(
          WORKSPACE_CURRENT_PREF
        );

        if (currentWorkspace == label) {
          Services.prefs.setStringPref(WORKSPACE_CURRENT_PREF, input.value);
        }
      } else if (result === false) {
        /* empty */
      } else {
        prompts.alert(
          null,
          l10n.formatValueSync("workspace-prompt-title"),
          l10n.formatValueSync("workspace-error") +
            "\n" +
            l10n.formatValueSync("workspace-error-discription")
        );
      }
    },

    // eslint-disable-next-line no-dupe-keys
    addNewWorkspace() {
      let allWorkspace = Services.prefs
        .getStringPref(WORKSPACE_ALL_PREF)
        .split(",");
      let l10n = new Localization(["browser/floorp.ftl"], true);
      prompts = Services.prompt;
      let check = { value: false };
      const pattern = /^[\p{L}\p{N}\s]+$/u;
      let input = { value: "" };
      let result = prompts.prompt(
        null,
        l10n.formatValueSync("workspace-prompt-title"),
        l10n.formatValueSync("please-enter-workspace-name") +
          "\n" +
          l10n.formatValueSync("please-enter-workspace-name-2"),
        input,
        null,
        check
      );

      if (
        result &&
        !allWorkspace.includes(input.value) &&
        input.value != "" &&
        input.value.length < 20 &&
        input.value != l10n.formatValueSync("workspace-default") &&
        pattern.test(input.value)
      ) {
        let label = input.value;
        label = label.replace(/\s+/g, "-");

        let workspaceAll = Services.prefs
          .getStringPref(WORKSPACE_ALL_PREF)
          .split(",");
        try {
          workspaceFunctions.WorkspaceContextMenu.addWorkspaceElemToMenu(label);
        } catch (e) {
          prompts.alert(
            null,
            l10n.formatValueSync("workspace-prompt-title"),
            l10n.formatValueSync("workspace-error") +
              "\n" +
              l10n.formatValueSync("workspace-error-discription")
          );
          return;
        }
        workspaceAll.push(label);
        Services.prefs.setStringPref(WORKSPACE_ALL_PREF, workspaceAll);
      } else if (result === false) {
        /* empty */
      } else {
        prompts.alert(
          null,
          l10n.formatValueSync("workspace-prompt-title"),
          l10n.formatValueSync("workspace-error") +
            "\n" +
            l10n.formatValueSync("workspace-error-discription")
        );
      }
    },

    setCurrentWorkspace() {
      let tabs = gBrowser.tabs;
      let currentWorkspace = Services.prefs.getStringPref(
        WORKSPACE_CURRENT_PREF
      );

      document
        .querySelector(`[floorp-lastVisibleTab]`)
        ?.removeAttribute("floorp-lastVisibleTab");
      document
        .querySelector(`[floorp-firstVisibleTab]`)
        ?.removeAttribute("floorp-firstVisibleTab");
      let lastTab = null;
      let firstTab = null;
      for (let i = 0; i < tabs.length; i++) {
        let tab = tabs[i];
        let workspace = tab.getAttribute("floorp-workspace");
        if (
          workspace == currentWorkspace ||
          !Services.prefs.getBoolPref(WORKSPACE_TAB_ENABLED_PREF)
        ) {
          gBrowser.showTab(tab);
          lastTab = tab;
          if (firstTab == null) {
            tab.setAttribute("floorp-firstVisibleTab", "true");
            firstTab = tab;
          }
        } else {
          gBrowser.hideTab(tab);
        }

        if (currentWorkspace != l10n.formatValueSync("workspace-default")) {
          document
            .getElementById("workspace-button")
            .setAttribute("label", currentWorkspace.replace(/-/g, " "));
          document.querySelector(
            "#workspace-button > .toolbarbutton-text"
          ).style.display = "inherit";
        } else {
          document.getElementById("workspace-button").removeAttribute("label");
          document.querySelector(
            "#workspace-button > .toolbarbutton-text"
          ).style.display = "none";
        }
      }
      lastTab?.setAttribute("floorp-lastVisibleTab", "true");
    },

    saveWorkspaceState() {
      let tabs = gBrowser.tabs;
      let tabStateObject = [];
      for (let i = 0; i < tabs.length; i++) {
        let tab = tabs[i];
        let tabState = {
          [i]: {
            workspace: tab.getAttribute("floorp-workspace"),
          },
        };
        tabStateObject.push(tabState);
      }
      Services.prefs.setStringPref(
        WORKSPACE_TABS_PREF,
        JSON.stringify(tabStateObject)
      );
    },

    changeWorkspaceToAfterNext() {
      let currentWorkspace = Services.prefs.getStringPref(
        WORKSPACE_CURRENT_PREF
      );
      let workspaceAll = Services.prefs
        .getStringPref(WORKSPACE_ALL_PREF)
        .split(",");
      let index = workspaceAll.indexOf(currentWorkspace);
      if (index == workspaceAll.length - 1) {
        return;
      }
      let afterWorkspace = workspaceAll[index + 1];
      workspaceFunctions.manageWorkspaceFunctions.changeWorkspace(
        afterWorkspace
      );
      workspaceFunctions.manageWorkspaceFunctions.setCurrentWorkspace();
    },

    changeWorkspaceToNext() {
      let allWorkspaces = Services.prefs
        .getCharPref(WORKSPACE_ALL_PREF)
        .split(",");
      let currentWorkspace = Services.prefs.getStringPref(
        WORKSPACE_CURRENT_PREF
      );
      let index = allWorkspaces.indexOf(currentWorkspace);
      let nextWorkspace = allWorkspaces[index + 1] ?? allWorkspaces[0];
      workspaceFunctions.manageWorkspaceFunctions.changeWorkspace(
        nextWorkspace
      );
    },

    changeWorkspaceToBeforeNext() {
      let currentWorkspace = Services.prefs.getStringPref(
        WORKSPACE_CURRENT_PREF
      );
      let workspaceAll = Services.prefs
        .getStringPref(WORKSPACE_ALL_PREF)
        .split(",");
      let index = workspaceAll.indexOf(currentWorkspace);
      if (index == 0) {
        return;
      }
      let beforeWorkspace = workspaceAll[index - 1];

      workspaceFunctions.manageWorkspaceFunctions.changeWorkspace(
        beforeWorkspace
      );
      workspaceFunctions.manageWorkspaceFunctions.setCurrentWorkspace();
    },

    changeWorkspace(label) {
      let tabs = gBrowser.tabs;

      Services.prefs.setStringPref(WORKSPACE_CURRENT_PREF, label);
      if (
        !workspaceFunctions.tabFunctions.checkWorkspaceLastShowedTabAttributeExist(
          label
        )
      ) {
        document
          .querySelector(`[floorp-workspace="${label}"]`)
          ?.setAttribute(`lastShowWorkspaceTab-${label}`, "true");
      }
      for (let i = 0; i < tabs.length; i++) {
        let tab = tabs[i];
        if (tab.getAttribute(`lastShowWorkspaceTab-${label}`) == "true") {
          gBrowser.selectedTab = tab;
          break;
        } else if (i == tabs.length - 1) {
          let newtabURL = Services.prefs.getStringPref(
            "browser.startup.homepage"
          );
          gBrowser.addTab(newtabURL, {
            skipAnimation: true,
            inBackground: false,
            triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          });
        }
      }
      workspaceFunctions.manageWorkspaceFunctions.setCurrentWorkspace();
      workspaceFunctions.manageWorkspaceFunctions.saveWorkspaceState();
    },
  },

  tabFunctions: {
    moveTabToOtherWorkspace(tab, workspace) {
      //checkTabisCurrentTab
      if (
        tab == gBrowser.selectedTab &&
        workspaceFunctions.tabFunctions.getNextToWorkspaceTab() != null
      ) {
        gBrowser.selectedTab = workspaceFunctions.tabFunctions.getNextToWorkspaceTab();
      }

      let willMoveWorkspace = workspace;
      tab.setAttribute("floorp-workspace", willMoveWorkspace);

      workspaceFunctions.manageWorkspaceFunctions.saveWorkspaceState();
      workspaceFunctions.manageWorkspaceFunctions.setCurrentWorkspace();
    },

    addLastShowedWorkspaceTab() {
      //add lastShowWorkspaceTab
      let currentTab = gBrowser.selectedTab;
      let currentWorkspace = Services.prefs.getStringPref(
        WORKSPACE_CURRENT_PREF
      );

      document
        .querySelector(`[lastShowWorkspaceTab-${currentWorkspace}]`)
        ?.removeAttribute(`lastShowWorkspaceTab-${currentWorkspace}`);
      currentTab.setAttribute(
        `lastShowWorkspaceTab-${currentWorkspace}`,
        "true"
      );
    },

    checkTabsLength() {
      let tabs = gBrowser.tabs;
      for (let i = 0; i < tabs.length; i++) {
        if (
          TabContextMenu.contextTab.getAttribute("firstVisibleTab") == "true"
        ) {
          document
            .getElementById("context_MoveOtherWorkspace")
            .setAttribute("disabled", "true");
        }
      }
    },

    getNextToWorkspaceTab() {
      let tabs = gBrowser.tabs;
      let nextTab = null;
      for (let i = 0; i < tabs.length; i++) {
        let tab = tabs[i];
        if (
          tab.getAttribute("floorp-workspace") ==
            gBrowser.selectedTab.getAttribute("floorp-workspace") &&
          tab != gBrowser.selectedTab
        ) {
          nextTab = tab;
          break;
        }
      }
      return nextTab;
    },

    checkWorkspaceLastShowedTabAttributeExist(label) {
      let tabs = gBrowser.tabs;
      let exist = false;
      for (let i = 0; i < tabs.length; i++) {
        let tab = tabs[i];
        if (tab.getAttribute(`lastShowWorkspaceTab-${label}`) == "true") {
          exist = true;
          break;
        }
      }
      return exist;
    },
  },

  TabContextMenu: {
    addContextMenuToTabContext() {
      let beforeElem = document.getElementById("context_moveTabOptions");
      let menuitemElem = window.MozXULElement.parseXULToFragment(`
      <menu id="context_MoveOtherWorkspace" data-l10n-id="move-tab-another-workspace" accesskey="D">
          <menupopup id="workspaceTabContextMenu"
                     onpopupshowing="workspaceFunctions.TabContextMenu.CreateWorkspaceContextMenu();"/>
      </menu>
      `);
      beforeElem.before(menuitemElem);
    },

    CreateWorkspaceContextMenu() {
      //delete already exsist items
      let menuElem = document.getElementById("workspaceTabContextMenu");
      while (menuElem.firstChild) {
        menuElem.firstChild.remove();
      }

      //Rebuild context menu
      if (workspaceFunctions.tabFunctions.getNextToWorkspaceTab() == null) {
        let menuItem = window.MozXULElement.parseXULToFragment(`
         <menuitem data-l10n-id="workspace-context-menu-selected-tab" disabled="true"/>
        `);
        let parentElem = document.getElementById("workspaceTabContextMenu");
        parentElem.appendChild(menuItem);
        return;
      }

      let workspaceAll = Services.prefs
        .getStringPref(WORKSPACE_ALL_PREF)
        .split(",");
      for (let i = 0; i < workspaceAll.length; i++) {
        let workspace = workspaceAll[i];
        let menuItem = window.MozXULElement.parseXULToFragment(`
          <menuitem id="workspaceID-${workspace}" class="workspaceContextMenuItems"
                    label="${workspace}"  oncommand="workspaceFunctions.tabFunctions.moveTabToOtherWorkspace(TabContextMenu.contextTab, '${workspace}');"/>
        `);
        let parentElem = document.getElementById("workspaceTabContextMenu");
        if (
          workspace !=
          TabContextMenu.contextTab.getAttribute("floorp-workspace")
        ) {
          parentElem.appendChild(menuItem);
        }
      }
    },
  },

  WorkspaceContextMenu: {
    addWorkspaceElemToMenu(label, nextElem) {
      let labelDisplay = label.replace(/-/g, " ");
      let workspaceItemElem = window.MozXULElement.parseXULToFragment(`

      <vbox id="workspace-box-${label}" class="workspace-label-box">
       <hbox id="workspace-${label}" class="workspace-item-box">
         <toolbarbutton id="workspace-label" label="${labelDisplay}"
                   class="toolbarbutton-1 workspace-item" workspace="${label}"
                   context="workspace-item-context" oncommand="workspaceFunctions.manageWorkspaceFunctions.changeWorkspace('${label}')"/>
       </hbox>
       <menuseparator class="workspace-item-separator"/>
      </vbox>
    `);

      if (nextElem) {
        nextElem.before(workspaceItemElem);
      } else {
        document
          .getElementById("addNewWorkspaceButton")
          .before(workspaceItemElem);
      }

      if (label !== defaultWorkspaceName) {
        let deleteButtonElem = window.MozXULElement.parseXULToFragment(`
            <toolbarbutton id="workspace-delete" class="workspace-item-delete toolbarbutton-1"
                           oncommand="workspaceFunctions.manageWorkspaceFunctions.deleteworkspace('${label}')"/>
        `);
        document
          .getElementById(`workspace-${label}`)
          .appendChild(deleteButtonElem);
      }
    },

    createWorkspacemenuItemContext(e) {
      let oldMenuItems = document.querySelectorAll(".workspace-item-contexts");

      for (let i = 0; i < oldMenuItems.length; i++) {
        oldMenuItems[i].remove();
      }

      let menuitemElem = window.MozXULElement.parseXULToFragment(`
      <menuitem class="workspace-item-contexts" id="workspace-item-context-rename" data-l10n-id="workspace-rename" oncommand="workspaceFunctions.manageWorkspaceFunctions.renameWorkspace('${e.explicitOriginalTarget.getAttribute(
        "label"
      )}')"/>
      `);

      if (
        e.explicitOriginalTarget.getAttribute("label") !== defaultWorkspaceName
      ) {
        document
          .getElementById("workspace-item-context")
          .appendChild(menuitemElem);
      }
    },

    setMenuItemCheckCSS() {
      let currentWorkspace = Services.prefs.getStringPref(
        WORKSPACE_CURRENT_PREF
      );
      document.getElementById("workspaceMenuItemCheckCSS")?.remove();

      let Tag = document.createElement("style");
      Tag.innerText = `
        .workspace-item[workspace="${currentWorkspace}"] > .toolbarbutton-icon {
          visibility: inherit !important;
        }
      `;
      Tag.setAttribute("id", "workspaceMenuItemCheckCSS");
      document.head.appendChild(Tag);
    },
  },
};

window.setTimeout(function() {
  let list = Services.wm.getEnumerator("navigator:browser");
  while (list.hasMoreElements()) {
    if (list.getNext() != window) {
      return;
    }
  }

  //run codes
  workspaceFunctions.manageWorkspaceFunctions.initWorkspace();
  workspaceFunctions.manageWorkspaceFunctions.setCurrentWorkspace();

  gBrowser.tabContainer.addEventListener(
    "TabOpen",
    workspaceFunctions.eventListeners.tabAddEventListeners.handleTabOpen
  );
  gBrowser.tabContainer.addEventListener(
    "TabClose",
    workspaceFunctions.eventListeners.tabAddEventListeners.handleTabClose
  );
  gBrowser.tabContainer.addEventListener(
    "TabMove",
    workspaceFunctions.eventListeners.tabAddEventListeners.handleTabMove
  );
  gBrowser.tabContainer.addEventListener(
    "TabSelect",
    workspaceFunctions.eventListeners.tabAddEventListeners.handleTabSelect
  );

  Services.prefs.addObserver(
    WORKSPACE_CURRENT_PREF,
    workspaceFunctions.eventListeners.prefsEventListeners
      .handleWorkspacePrefChange
  );
  Services.prefs.addObserver(
    WORKSPACE_TAB_ENABLED_PREF,
    workspaceFunctions.eventListeners.prefsEventListeners
      .handleWorkspaceTabPrefChange
  );

  document.addEventListener(
    "keydown",
    workspaceFunctions.eventListeners.keyboradEventListeners.handle_keydown
  );
}, 1000);
