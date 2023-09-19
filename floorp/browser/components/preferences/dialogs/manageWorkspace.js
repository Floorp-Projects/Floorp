/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from /toolkit/content/preferencesBindings.js */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs"
);

const WorkspaceUtils = ChromeUtils.importESModule(
  "resource:///modules/WorkspaceUtils.sys.mjs"
);

XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization([
    "branding/brand.ftl",
    "browser/floorp",
  ]);
});

function setTitle() {
    let winElem = document.documentElement;
      document.l10n.setAttributes(winElem, "workspace-customize");
 }
 setTitle();
  
  async function onLoad() {
    const workspaces = Services.prefs.getStringPref(WorkspaceUtils.workspacesPreferences.WORKSPACE_ALL_PREF).split(",");

    const workspaceSelect = document.getElementById("workspacesPopup");
    const workspaceNameLabel = document.getElementById("workspaceName");
    for (let i = 0; i < workspaces.length; i++) {
      const workspace = workspaces[i].replace(/-/g, " ");

      const element = window.MozXULElement.parseXULToFragment(`
        <menuitem label="${workspace}" value="${workspace}"></menuitem>
      `);

        workspaceSelect.appendChild(element);
    }
    if(window.arguments != undefined) {
      workspaceNameLabel.value = window.arguments[0].workspaceName;
    } else {
      workspaceNameLabel.value = WorkspaceUtils.defaultWorkspaceName;
    }

    //icon
    const iconSelect = document.getElementById("workspacesIconSelectPopup");
    const iconNameLabel = document.getElementById("iconName");

    for (let icon of WorkspaceUtils.CONTAINER_ICONS) {
      const element = window.MozXULElement.parseXULToFragment(`
        <menuitem value="${icon}" data-l10n-id="workspace-icon-${icon}"
                  style="list-style-image: url(chrome://browser/skin/workspace-icons/${icon}.svg);">
        </menuitem>
      `);

      iconSelect.appendChild(element);
    }
    iconNameLabel.value = "fingerprint";

    // container
    const containerSelect = document.getElementById("workspacesContainerSelectPopup");
    const containerNameLabel = document.getElementById("containerName");
    const currentContainers = ContextualIdentityService.getPublicIdentities();

    for (let container of currentContainers) {
      const element = window.MozXULElement.parseXULToFragment(`
        <menuitem label="${container.name}" value="${container.userContextId}"></menuitem>
      `);

      if (container.l10nID) {
        let labelName = ContextualIdentityService.getUserContextLabel(container.userContextId);
        element.firstElementChild.setAttribute("label", labelName);
      }
      
      containerSelect.appendChild(element);
    }

    const noContainer = window.MozXULElement.parseXULToFragment(`
      <menuitem label="No Container" value="0"></menuitem>
    `);
    containerSelect.appendChild(noContainer);
    containerNameLabel.value = "0";

    document.addEventListener("dialogaccept", setPref);
  }  
  
  function setPref() {
    const workspaceNameLabel = document.getElementById("workspaceName").value.replace(/\s+/g, "-")
    const iconNameLabel = document.getElementById("iconName").value;
    const containerNameLabel = document.getElementById("containerName").value;

    // Return object with workspace name and icon name
    Services.obs.notifyObservers({ name: workspaceNameLabel, icon: iconNameLabel, container: containerNameLabel }, "addConfigToWorkspace");
  }
