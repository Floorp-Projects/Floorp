/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource:///modules/SitePermissions.jsm");

function Permission(principal, type, capability, capabilityString) {
  this.principal = principal;
  this.origin = principal.origin;
  this.type = type;
  this.capability = capability;
  this.capabilityString = capabilityString;
}

var gSitePermissionsManager = {
  _type: "",
  _isObserving: false,
  _permissions: new Map(),
  _permissionsToChange: new Map(),
  _permissionsToDelete: new Map(),
  _list: null,
  _bundle: null,
  _removeButton: null,
  _removeAllButton: null,

  onLoad() {
    let params = window.arguments[0];
    this.init(params);
  },

  init(params) {
    if (!this._isObserving) {
      Services.obs.addObserver(this, "perm-changed");
      this._isObserving = true;
    }

    this._bundle = document.getElementById("bundlePreferences");
    this._type = params.permissionType;
    this._list = document.getElementById("permissionsBox");
    this._removeButton = document.getElementById("removePermission");
    this._removeAllButton = document.getElementById("removeAllPermissions");

    let permissionsText = document.getElementById("permissionsText");
    while (permissionsText.hasChildNodes())
      permissionsText.firstChild.remove();
    permissionsText.appendChild(document.createTextNode(params.introText));

    document.title = params.windowTitle;

    this._loadPermissions();
  },

  uninit() {
    if (this._isObserving) {
      Services.obs.removeObserver(this, "perm-changed");
      this._isObserving = false;
    }
  },

  observe(subject, topic, data) {
    if (topic !== "perm-changed")
      return;

    let permission = subject.QueryInterface(Components.interfaces.nsIPermission);

    // Ignore unrelated permission types.
    if (permission.type !== this._type)
      return;

    if (data == "added") {
      this._addPermissionToList(permission);
    } else if (data == "changed") {
      let p = this._permissions.get(permission.principal.origin);
      p.capability = permission.capability;
      p.capabilityString = this._getCapabilityString(permission.capability);
      this._handleCapabilityChange(p);
    } else if (data == "deleted") {
      this._removePermissionFromList(permission.principal.origin);
    }
  },

  _handleCapabilityChange(perm) {
    let permissionlistitem = document.getElementsByAttribute("origin", perm.origin)[0];
    let menulist = permissionlistitem.getElementsByTagName("menulist")[0];
    menulist.selectedItem =
      menulist.getElementsByAttribute("value", perm.capability)[0];
  },

  _getCapabilityString(capability) {
    let stringKey = null;
    switch (capability) {
    case Services.perms.ALLOW_ACTION:
      stringKey = "can";
      break;
    case Services.perms.DENY_ACTION:
      stringKey = "cannot";
      break;
    case Services.perms.PROMPT_ACTION:
      stringKey = "prompt"
      break;
    }
    return this._bundle.getString(stringKey);
  },

  _addPermissionToList(perm) {
    if (perm.type !== this._type)
      return;
    let capabilityString = this._getCapabilityString(perm.capability);
    let p = new Permission(perm.principal, perm.type, perm.capability,
                           capabilityString);
    this._permissions.set(p.origin, p);
    this._createPermissionListItem(p);
  },

  _removePermissionFromList(origin) {
    this._permissions.delete(origin);
    let permissionlistitem = document.getElementsByAttribute("origin", origin)[0];
    this._list.removeItemAt(this._list.getIndexOfItem(permissionlistitem));
  },

  _loadPermissions() {
    // load permissions into a table.
    let enumerator = Services.perms.enumerator;
    while (enumerator.hasMoreElements()) {
      let nextPermission = enumerator.getNext().QueryInterface(Components.interfaces.nsIPermission);
      this._addPermissionToList(nextPermission);
    }

    // disable "remove all" button if there are none
    this._setRemoveButtonState();
  },

  _createPermissionListItem(permission) {
    let richlistitem = document.createElement("richlistitem");
    richlistitem.setAttribute("origin", permission.origin);
    let row = document.createElement("hbox");
    row.setAttribute("flex", "1");

    let hbox = document.createElement("hbox");
    let website = document.createElement("label");
    website.setAttribute("value", permission.origin);
    website.setAttribute("width", "50");
    hbox.setAttribute("class", "website-name");
    hbox.setAttribute("flex", "3");
    hbox.appendChild(website);

    let menulist = document.createElement("menulist");
    let menupopup = document.createElement("menupopup");
    menulist.setAttribute("flex", "1");
    menulist.setAttribute("width", "50");
    menulist.appendChild(menupopup);
    let states = SitePermissions.getAvailableStates(permission.type);
    for (let state of states) {
      if (state == SitePermissions.UNKNOWN)
        continue;
      let m = document.createElement("menuitem");
      m.setAttribute("label", this._getCapabilityString(state));
      m.setAttribute("value", state);
      menupopup.appendChild(m);
    }
    menulist.value = permission.capability;

    menulist.addEventListener("select", () => {
      this.onPermissionChange(permission, Number(menulist.selectedItem.value));
    });

    row.appendChild(hbox);
    row.appendChild(menulist);
    richlistitem.appendChild(row);
    this._list.appendChild(richlistitem)
  },

  onWindowKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_ESCAPE)
      window.close();
  },

  onPermissionKeyPress(event) {
    if (!this._list.selectedItem)
      return;

    if (event.keyCode == KeyEvent.DOM_VK_DELETE ||
       (AppConstants.platform == "macosx" &&
        event.keyCode == KeyEvent.DOM_VK_BACK_SPACE)) {
      this.onPermissionDelete();
      event.preventDefault();
    }
  },

  _setRemoveButtonState() {
    if (!this._list)
      return;

    let hasSelection = this._list.selectedIndex;
    let hasRows = this._list.itemCount > 1;
    this._removeButton.disabled = !hasSelection;
    this._removeAllButton.disabled = !hasRows;
  },

  onPermissionDelete() {
    let richlistitem = this._list.selectedItem;
    let origin = richlistitem.getAttribute("origin");
    let permission = this._permissions.get(origin);

    this._removePermissionFromList(origin);
    this._permissionsToDelete.set(permission.origin, permission);

    this._setRemoveButtonState();
  },

  onAllPermissionsDelete() {
    for (let permission of this._permissions.values()) {
      this._removePermissionFromList(permission.origin);
      this._permissionsToDelete.set(permission.origin, permission);
    }

    this._setRemoveButtonState();
  },

  onPermissionSelect() {
    this._setRemoveButtonState();
  },

  onPermissionChange(perm, capability) {
    let p = this._permissions.get(perm.origin);
    if (p.capability == capability)
      return;
    p.capability = capability;
    p.capabilityString = this._getCapabilityString(capability);
    this._permissionsToChange.set(p.origin, p);

    // enable "remove all" button as needed
    this._setRemoveButtonState();
  },

  onApplyChanges() {
    // Stop observing permission changes since we are about
    // to write out the pending adds/deletes and don't need
    // to update the UI
    this.uninit();

    for (let p of this._permissionsToChange.values()) {
      let uri = Services.io.newURI(p.origin);
      SitePermissions.set(uri, p.type, p.capability);
    }

    for (let p of this._permissionsToDelete.values()) {
      let uri = Services.io.newURI(p.origin);
      SitePermissions.remove(uri, p.type);
    }
    window.close();
  },
};
