/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu } = Components;
const DISABLED_TOOLS = "devtools.toolbox.disabledTools";
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  setupToolsList();
  populatePreferences();
});

function setupToolsList() {
  let disabledTools = [];
  try {
    disabledTools = JSON.parse(Services.prefs.getCharPref(DISABLED_TOOLS));
  } catch(ex) {
    Cu.reportError("Error parsing pref " + DISABLED_TOOLS + " as JSON.");
  }
  let defaultToolsBox = document.getElementById("default-tools-box");
  let additionalToolsBox = document.getElementById("additional-tools-box");

  defaultToolsBox.textContent = "";
  additionalToolsBox.textContent = "";

  let onCheckboxClick = function(id) {
    if (disabledTools.indexOf(id) > -1) {
      disabledTools.splice(disabledTools.indexOf(id), 1);
      Services.prefs.setCharPref(DISABLED_TOOLS, JSON.stringify(disabledTools));
      gDevTools.emit("tool-registered", id);
    }
    else {
      disabledTools.push(id);
      Services.prefs.setCharPref(DISABLED_TOOLS, JSON.stringify(disabledTools));
      gDevTools.emit("tool-unregistered", gDevTools._tools.get(id));
    }
  };

  // Populating the default tools lists
  for (let tool of gDevTools.getDefaultTools()) {
    let checkbox = document.createElement("checkbox");
    checkbox.setAttribute("id", tool.id);
    checkbox.setAttribute("label", tool.label);
    checkbox.setAttribute("tooltiptext", tool.tooltip || "");
    checkbox.setAttribute("checked", disabledTools.indexOf(tool.id) == -1);
    checkbox.addEventListener("command", onCheckboxClick.bind(null, tool.id));
    defaultToolsBox.appendChild(checkbox);
  }

  // Populating the additional tools list that came from add-ons.
  let atleastOneAddon = false;
  for (let tool of gDevTools.getAdditionalTools()) {
    atleastOneAddon = true;
    let checkbox = document.createElement("checkbox");
    checkbox.setAttribute("id", tool.id);
    checkbox.setAttribute("label", tool.label);
    checkbox.setAttribute("tooltiptext", tool.tooltip || "");
    checkbox.setAttribute("checked", disabledTools.indexOf(tool.id) == -1);
    checkbox.addEventListener("command", onCheckboxClick.bind(null, tool.id));
    additionalToolsBox.appendChild(checkbox);
  }

  if (!atleastOneAddon) {
    additionalToolsBox.style.display = "none";
    additionalToolsBox.previousSibling.style.display = "none";
  }

  window.focus();
}

function populatePreferences() {
  let prefCheckboxes = document.querySelectorAll("checkbox[data-pref]");
  for (let checkbox of prefCheckboxes) {
    checkbox.checked = Services.prefs.getBoolPref(checkbox.getAttribute("data-pref"));
    checkbox.addEventListener("command", function() {
      let data = {
        pref: this.getAttribute("data-pref"),
        newValue: this.checked
      };
      data.oldValue = Services.prefs.getBoolPref(data.pref);
      Services.prefs.setBoolPref(data.pref, data.newValue);
      gDevTools.emit("pref-changed", data);
    }.bind(checkbox));
  }
  let prefRadiogroups = document.querySelectorAll("radiogroup[data-pref]");
  for (let radiogroup of prefRadiogroups) {
    let selectedValue = Services.prefs.getCharPref(radiogroup.getAttribute("data-pref"));
    for (let radio of radiogroup.childNodes) {
      radiogroup.selectedIndex = -1;
      if (radio.getAttribute("value") == selectedValue) {
        radiogroup.selectedItem = radio;
        break;
      }
    }
    radiogroup.addEventListener("select", function() {
      let data = {
        pref: this.getAttribute("data-pref"),
        newValue: this.selectedItem.getAttribute("value")
      };
      data.oldValue = Services.prefs.getCharPref(data.pref);
      Services.prefs.setCharPref(data.pref, data.newValue);
      gDevTools.emit("pref-changed", data);
    }.bind(radiogroup));
  }
}
