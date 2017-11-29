/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from pageInfo.js */

Components.utils.import("resource:///modules/SitePermissions.jsm");
Components.utils.import("resource://gre/modules/BrowserUtils.jsm");

var gPermURI;
var gPermPrincipal;
var gUsageRequest;

// Array of permissionIDs sorted alphabetically by label.
var gPermissions = SitePermissions.listPermissions().sort((a, b) => {
  let firstLabel = SitePermissions.getPermissionLabel(a);
  let secondLabel = SitePermissions.getPermissionLabel(b);
  return firstLabel.localeCompare(secondLabel);
});
gPermissions.push("plugins");

var permissionObserver = {
  observe(aSubject, aTopic, aData) {
    if (aTopic == "perm-changed") {
      var permission = aSubject.QueryInterface(Components.interfaces.nsIPermission);
      if (permission.matchesURI(gPermURI, true)) {
        if (gPermissions.indexOf(permission.type) > -1)
          initRow(permission.type);
        else if (permission.type.startsWith("plugin"))
          setPluginsRadioState();
      }
    }
  }
};

function onLoadPermission(uri, principal) {
  var permTab = document.getElementById("permTab");
  if (SitePermissions.isSupportedURI(uri)) {
    gPermURI = uri;
    gPermPrincipal = principal;
    var hostText = document.getElementById("hostText");
    hostText.value = gPermURI.displayPrePath;

    for (var i of gPermissions)
      initRow(i);
    Services.obs.addObserver(permissionObserver, "perm-changed");
    onUnloadRegistry.push(onUnloadPermission);
    permTab.hidden = false;
  } else
    permTab.hidden = true;
}

function onUnloadPermission() {
  Services.obs.removeObserver(permissionObserver, "perm-changed");

  if (gUsageRequest) {
    gUsageRequest.cancel();
    gUsageRequest = null;
  }
}

function initRow(aPartId) {
  if (aPartId == "plugins") {
    initPluginsRow();
    return;
  }

  createRow(aPartId);

  var checkbox = document.getElementById(aPartId + "Def");
  var command  = document.getElementById("cmd_" + aPartId + "Toggle");
  var {state} = SitePermissions.get(gPermURI, aPartId);
  let defaultState = SitePermissions.getDefault(aPartId);

  if (state != defaultState) {
    checkbox.checked = false;
    command.removeAttribute("disabled");
  } else {
    checkbox.checked = true;
    command.setAttribute("disabled", "true");
  }
  setRadioState(aPartId, state);
}

function createRow(aPartId) {
  let rowId = "perm-" + aPartId + "-row";
  if (document.getElementById(rowId))
    return;

  let commandId = "cmd_" + aPartId + "Toggle";
  let labelId = "perm-" + aPartId + "-label";
  let radiogroupId = aPartId + "RadioGroup";

  let command = document.createElement("command");
  command.setAttribute("id", commandId);
  command.setAttribute("oncommand", "onRadioClick('" + aPartId + "');");
  document.getElementById("pageInfoCommandSet").appendChild(command);

  let row = document.createElement("vbox");
  row.setAttribute("id", rowId);
  row.setAttribute("class", "permission");

  let label = document.createElement("label");
  label.setAttribute("id", labelId);
  label.setAttribute("control", radiogroupId);
  label.setAttribute("value", SitePermissions.getPermissionLabel(aPartId));
  label.setAttribute("class", "permissionLabel");
  row.appendChild(label);

  let controls = document.createElement("hbox");
  controls.setAttribute("role", "group");
  controls.setAttribute("aria-labelledby", labelId);

  let checkbox = document.createElement("checkbox");
  checkbox.setAttribute("id", aPartId + "Def");
  checkbox.setAttribute("oncommand", "onCheckboxClick('" + aPartId + "');");
  checkbox.setAttribute("label", gBundle.getString("permissions.useDefault"));
  controls.appendChild(checkbox);

  let spacer = document.createElement("spacer");
  spacer.setAttribute("flex", "1");
  controls.appendChild(spacer);

  let radiogroup = document.createElement("radiogroup");
  radiogroup.setAttribute("id", radiogroupId);
  radiogroup.setAttribute("orient", "horizontal");
  for (let state of SitePermissions.getAvailableStates(aPartId)) {
    let radio = document.createElement("radio");
    radio.setAttribute("id", aPartId + "#" + state);
    radio.setAttribute("label", SitePermissions.getMultichoiceStateLabel(state));
    radio.setAttribute("command", commandId);
    radiogroup.appendChild(radio);
  }
  controls.appendChild(radiogroup);

  row.appendChild(controls);

  document.getElementById("permList").appendChild(row);
}

function onCheckboxClick(aPartId) {
  var command  = document.getElementById("cmd_" + aPartId + "Toggle");
  var checkbox = document.getElementById(aPartId + "Def");
  if (checkbox.checked) {
    SitePermissions.remove(gPermURI, aPartId);
    command.setAttribute("disabled", "true");
    var perm = SitePermissions.getDefault(aPartId);
    setRadioState(aPartId, perm);
  } else {
    onRadioClick(aPartId);
    command.removeAttribute("disabled");
  }
}

function onPluginRadioClick(aEvent) {
  onRadioClick(aEvent.originalTarget.getAttribute("id").split("#")[0]);
}

function onRadioClick(aPartId) {
  var radioGroup = document.getElementById(aPartId + "RadioGroup");
  var id = radioGroup.selectedItem.id;
  var permission = parseInt(id.split("#")[1]);
  SitePermissions.set(gPermURI, aPartId, permission);
}

function setRadioState(aPartId, aValue) {
  var radio = document.getElementById(aPartId + "#" + aValue);
  if (radio) {
    radio.radioGroup.selectedItem = radio;
  }
}

function fillInPluginPermissionTemplate(aPluginName, aPermissionString) {
  let permPluginTemplate = document.getElementById("permPluginTemplate").cloneNode(true);
  permPluginTemplate.setAttribute("permString", aPermissionString);
  let attrs = [
    [ ".permPluginTemplateLabel", "value", aPluginName ],
    [ ".permPluginTemplateRadioGroup", "id", aPermissionString + "RadioGroup" ],
    [ ".permPluginTemplateRadioDefault", "id", aPermissionString + "#0" ],
    [ ".permPluginTemplateRadioAsk", "id", aPermissionString + "#3" ],
    [ ".permPluginTemplateRadioAllow", "id", aPermissionString + "#1" ],
    // #8 comes from Ci.nsIObjectLoadingContent.PLUGIN_PERMISSION_PROMPT_ACTION_QUIET
    [ ".permPluginTemplateRadioHide", "id", aPermissionString + "#8"],
    [ ".permPluginTemplateRadioBlock", "id", aPermissionString + "#2" ]
  ];

  for (let attr of attrs) {
    permPluginTemplate.querySelector(attr[0]).setAttribute(attr[1], attr[2]);
  }

  return permPluginTemplate;
}

function clearPluginPermissionTemplate() {
  let permPluginTemplate = document.getElementById("permPluginTemplate");
  permPluginTemplate.hidden = true;
  permPluginTemplate.removeAttribute("permString");
  document.querySelector(".permPluginTemplateLabel").removeAttribute("value");
  document.querySelector(".permPluginTemplateRadioGroup").removeAttribute("id");
  document.querySelector(".permPluginTemplateRadioAsk").removeAttribute("id");
  document.querySelector(".permPluginTemplateRadioAllow").removeAttribute("id");
  document.querySelector(".permPluginTemplateRadioBlock").removeAttribute("id");
}

function initPluginsRow() {
  let vulnerableLabel = document.getElementById("browserBundle").getString("pluginActivateVulnerable.label");
  let pluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

  let permissionMap = new Map();

  for (let plugin of pluginHost.getPluginTags()) {
    if (plugin.disabled) {
      continue;
    }
    for (let mimeType of plugin.getMimeTypes()) {
      let permString = pluginHost.getPermissionStringForType(mimeType);
      if (!permissionMap.has(permString)) {
        let name = BrowserUtils.makeNicePluginName(plugin.name);
        if (permString.startsWith("plugin-vulnerable:")) {
          name += " \u2014 " + vulnerableLabel;
        }
        permissionMap.set(permString, name);
      }
    }
  }

  let entries = Array.from(permissionMap, item => ({ name: item[1], permission: item[0] }));

  entries.sort(function(a, b) {
    return a.name.localeCompare(b.name);
  });

  let permissionEntries = entries.map(p => fillInPluginPermissionTemplate(p.name, p.permission));

  let permPluginsRow = document.getElementById("perm-plugins-row");
  clearPluginPermissionTemplate();
  if (permissionEntries.length < 1) {
    permPluginsRow.hidden = true;
    return;
  }

  for (let permissionEntry of permissionEntries) {
    permPluginsRow.appendChild(permissionEntry);
  }

  setPluginsRadioState();
}

function setPluginsRadioState() {
  let box = document.getElementById("perm-plugins-row");
  for (let permissionEntry of box.childNodes) {
    if (permissionEntry.hasAttribute("permString")) {
      let permString = permissionEntry.getAttribute("permString");
      let permission = SitePermissions.get(gPermURI, permString);
      setRadioState(permString, permission.state);
    }
  }
}
