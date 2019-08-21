/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from pageInfo.js */

const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);

var gPermPrincipal;

// Array of permissionIDs sorted alphabetically by label.
var gPermissions = SitePermissions.listPermissions()
  .filter(p => SitePermissions.getPermissionLabel(p) != null)
  .sort((a, b) => {
    let firstLabel = SitePermissions.getPermissionLabel(a);
    let secondLabel = SitePermissions.getPermissionLabel(b);
    return firstLabel.localeCompare(secondLabel);
  });

var permissionObserver = {
  observe(aSubject, aTopic, aData) {
    if (aTopic == "perm-changed") {
      var permission = aSubject.QueryInterface(Ci.nsIPermission);
      if (
        permission.matches(gPermPrincipal, true) &&
        gPermissions.includes(permission.type)
      ) {
        initRow(permission.type);
      }
    }
  },
};

function onLoadPermission(uri, principal) {
  var permTab = document.getElementById("permTab");
  if (SitePermissions.isSupportedPrincipal(principal)) {
    gPermPrincipal = principal;
    var hostText = document.getElementById("hostText");
    hostText.value = uri.displayPrePath;

    for (var i of gPermissions) {
      initRow(i);
    }
    Services.obs.addObserver(permissionObserver, "perm-changed");
    onUnloadRegistry.push(onUnloadPermission);
    permTab.hidden = false;
  } else {
    permTab.hidden = true;
  }
}

function onUnloadPermission() {
  Services.obs.removeObserver(permissionObserver, "perm-changed");
}

function initRow(aPartId) {
  createRow(aPartId);

  var checkbox = document.getElementById(aPartId + "Def");
  var command = document.getElementById("cmd_" + aPartId + "Toggle");
  var { state, scope } = SitePermissions.getForPrincipal(
    gPermPrincipal,
    aPartId
  );
  let defaultState = SitePermissions.getDefault(aPartId);

  // Since cookies preferences have many different possible configuration states
  // we don't consider any permission except "no permission" to be default.
  if (aPartId == "cookie") {
    state = Services.perms.testPermissionFromPrincipal(
      gPermPrincipal,
      "cookie"
    );

    if (state == SitePermissions.UNKNOWN) {
      checkbox.checked = true;
      command.setAttribute("disabled", "true");
      // Don't select any item in the radio group, as we can't
      // confidently say that all cookies on the site will be allowed.
      let radioGroup = document.getElementById("cookieRadioGroup");
      radioGroup.selectedItem = null;
    } else {
      checkbox.checked = false;
      command.removeAttribute("disabled");
    }

    setRadioState(aPartId, state);
    return;
  }

  if (state != defaultState) {
    checkbox.checked = false;
    command.removeAttribute("disabled");
  } else {
    checkbox.checked = true;
    command.setAttribute("disabled", "true");
  }

  if (
    [SitePermissions.SCOPE_POLICY, SitePermissions.SCOPE_GLOBAL].includes(scope)
  ) {
    checkbox.setAttribute("disabled", "true");
    command.setAttribute("disabled", "true");
  }

  setRadioState(aPartId, state);
}

function createRow(aPartId) {
  let rowId = "perm-" + aPartId + "-row";
  if (document.getElementById(rowId)) {
    return;
  }

  let commandId = "cmd_" + aPartId + "Toggle";
  let labelId = "perm-" + aPartId + "-label";
  let radiogroupId = aPartId + "RadioGroup";

  let command = document.createXULElement("command");
  command.setAttribute("id", commandId);
  command.setAttribute("oncommand", "onRadioClick('" + aPartId + "');");
  document.getElementById("pageInfoCommandSet").appendChild(command);

  let row = document.createXULElement("vbox");
  row.setAttribute("id", rowId);
  row.setAttribute("class", "permission");

  let label = document.createXULElement("label");
  label.setAttribute("id", labelId);
  label.setAttribute("control", radiogroupId);
  label.setAttribute("value", SitePermissions.getPermissionLabel(aPartId));
  label.setAttribute("class", "permissionLabel");
  row.appendChild(label);

  let controls = document.createXULElement("hbox");
  controls.setAttribute("role", "group");
  controls.setAttribute("aria-labelledby", labelId);

  let checkbox = document.createXULElement("checkbox");
  checkbox.setAttribute("id", aPartId + "Def");
  checkbox.setAttribute("oncommand", "onCheckboxClick('" + aPartId + "');");
  document.l10n.setAttributes(checkbox, "permissions-use-default");
  controls.appendChild(checkbox);

  let spacer = document.createXULElement("spacer");
  spacer.setAttribute("flex", "1");
  controls.appendChild(spacer);

  let radiogroup = document.createXULElement("radiogroup");
  radiogroup.setAttribute("id", radiogroupId);
  radiogroup.setAttribute("orient", "horizontal");
  for (let state of SitePermissions.getAvailableStates(aPartId)) {
    let radio = document.createXULElement("radio");
    radio.setAttribute("id", aPartId + "#" + state);
    radio.setAttribute(
      "label",
      SitePermissions.getMultichoiceStateLabel(aPartId, state)
    );
    radio.setAttribute("command", commandId);
    radiogroup.appendChild(radio);
  }
  controls.appendChild(radiogroup);

  row.appendChild(controls);

  document.getElementById("permList").appendChild(row);
}

function onCheckboxClick(aPartId) {
  var command = document.getElementById("cmd_" + aPartId + "Toggle");
  var checkbox = document.getElementById(aPartId + "Def");
  if (checkbox.checked) {
    SitePermissions.removeFromPrincipal(gPermPrincipal, aPartId);
    command.setAttribute("disabled", "true");
  } else {
    onRadioClick(aPartId);
    command.removeAttribute("disabled");
  }
}

function onRadioClick(aPartId) {
  var radioGroup = document.getElementById(aPartId + "RadioGroup");
  var id = radioGroup.selectedItem ? radioGroup.selectedItem.id : "#1";
  var permission = parseInt(id.split("#")[1]);
  SitePermissions.setForPrincipal(gPermPrincipal, aPartId, permission);
}

function setRadioState(aPartId, aValue) {
  var radio = document.getElementById(aPartId + "#" + aValue);
  if (radio) {
    radio.radioGroup.selectedItem = radio;
  }
}
