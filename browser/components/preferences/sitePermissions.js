/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource:///modules/SitePermissions.jsm");

const sitePermissionsL10n = {
  "desktop-notification": {
    window: "permissions-site-notification-window",
    description: "permissions-site-notification-desc",
    disableLabel: "permissions-site-notification-disable-label",
    disableDescription: "permissions-site-notification-disable-desc",
  },
  "geo": {
    window: "permissions-site-location-window",
    description: "permissions-site-location-desc",
    disableLabel: "permissions-site-location-disable-label",
    disableDescription: "permissions-site-location-disable-desc",
  },
  "camera": {
    window: "permissions-site-camera-window",
    description: "permissions-site-camera-desc",
    disableLabel: "permissions-site-camera-disable-label",
    disableDescription: "permissions-site-camera-disable-desc",
  },
  "microphone": {
    window: "permissions-site-microphone-window",
    description: "permissions-site-microphone-desc",
    disableLabel: "permissions-site-microphone-disable-label",
    disableDescription: "permissions-site-microphone-disable-desc",
  },
};

function Permission(principal, type, capability, l10nId) {
  this.principal = principal;
  this.origin = principal.origin;
  this.type = type;
  this.capability = capability;
  this.l10nId = l10nId;
}

const PERMISSION_STATES = [SitePermissions.ALLOW, SitePermissions.BLOCK, SitePermissions.PROMPT];

var gSitePermissionsManager = {
  _type: "",
  _isObserving: false,
  _permissions: new Map(),
  _permissionsToChange: new Map(),
  _permissionsToDelete: new Map(),
  _list: null,
  _removeButton: null,
  _removeAllButton: null,
  _searchBox: null,
  _checkbox: null,
  _currentDefaultPermissionsState: null,
  _defaultPermissionStatePrefName: null,

  onLoad() {
    let params = window.arguments[0];
    document.mozSubdialogReady = this.init(params);
  },

  async init(params) {
    if (!this._isObserving) {
      Services.obs.addObserver(this, "perm-changed");
      this._isObserving = true;
    }

    this._type = params.permissionType;
    this._list = document.getElementById("permissionsBox");
    this._removeButton = document.getElementById("removePermission");
    this._removeAllButton = document.getElementById("removeAllPermissions");
    this._searchBox = document.getElementById("searchBox");
    this._checkbox = document.getElementById("permissionsDisableCheckbox");

    let permissionsDisableDescription = document.getElementById("permissionsDisableDescription");
    let permissionsText = document.getElementById("permissionsText");

    const l10n = sitePermissionsL10n[this._type];
    document.l10n.setAttributes(permissionsText, l10n.description);
    document.l10n.setAttributes(this._checkbox, l10n.disableLabel);
    document.l10n.setAttributes(permissionsDisableDescription, l10n.disableDescription);
    document.l10n.setAttributes(document.documentElement, l10n.window);

    await document.l10n.translateElements([
      permissionsText,
      this._checkbox,
      permissionsDisableDescription,
      document.documentElement,
    ]);

    // Initialize the checkbox state.
    this._defaultPermissionStatePrefName = "permissions.default." + this._type;
    let pref = Services.prefs.getPrefType(this._defaultPermissionStatePrefName);
    if (pref != Services.prefs.PREF_INVALID) {
      this._currentDefaultPermissionsState = Services.prefs.getIntPref(this._defaultPermissionStatePrefName);
    }

    if (this._currentDefaultPermissionsState === null) {
      this._checkbox.setAttribute("hidden", true);
      permissionsDisableDescription.setAttribute("hidden", true);
    } else if (this._currentDefaultPermissionsState == SitePermissions.BLOCK) {
      this._checkbox.checked = true;
    } else {
      this._checkbox.checked = false;
    }

    this._loadPermissions();
    await this.buildPermissionsList();

    this._searchBox.focus();
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

    let permission = subject.QueryInterface(Ci.nsIPermission);

    // Ignore unrelated permission types and permissions with unknown states.
    if (permission.type !== this._type || !PERMISSION_STATES.includes(permission.capability))
      return;

    if (data == "added") {
      this._addPermissionToList(permission);
      this.buildPermissionsList();
    } else if (data == "changed") {
      let p = this._permissions.get(permission.principal.origin);
      p.capability = permission.capability;
      p.l10nId = this._getCapabilityString(permission.capability);
      this._handleCapabilityChange(p);
      this.buildPermissionsList();
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
      stringKey = "permissions-capabilities-allow";
      break;
    case Services.perms.DENY_ACTION:
      stringKey = "permissions-capabilities-block";
      break;
    case Services.perms.PROMPT_ACTION:
      stringKey = "permissions-capabilities-prompt";
      break;
    default:
        throw new Error(`Unknown capability: ${capability}`);
    }
    return stringKey;
  },

  _addPermissionToList(perm) {
    // Ignore unrelated permission types and permissions with unknown states.
    if (perm.type !== this._type || !PERMISSION_STATES.includes(perm.capability))
      return;
    let l10nId = this._getCapabilityString(perm.capability);
    let p = new Permission(perm.principal, perm.type, perm.capability, l10nId);
    this._permissions.set(p.origin, p);
  },

  _removePermissionFromList(origin) {
    this._permissions.delete(origin);
    let permissionlistitem = document.getElementsByAttribute("origin", origin)[0];
    if (permissionlistitem) {
      permissionlistitem.remove();
    }
  },

  _loadPermissions() {
    // load permissions into a table.
    let enumerator = Services.perms.enumerator;
    while (enumerator.hasMoreElements()) {
      let nextPermission = enumerator.getNext().QueryInterface(Ci.nsIPermission);
      this._addPermissionToList(nextPermission);
    }
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
    menulist.setAttribute("class", "website-status");
    menulist.appendChild(menupopup);
    let states = SitePermissions.getAvailableStates(permission.type);
    for (let state of states) {
      // Work around the (rare) edge case when a user has changed their
      // default permission type back to UNKNOWN while still having a
      // PROMPT permission set for an origin.
      if (state == SitePermissions.UNKNOWN &&
          permission.capability == SitePermissions.PROMPT) {
        state = SitePermissions.PROMPT;
      } else if (state == SitePermissions.UNKNOWN) {
        continue;
      }
      let m = document.createElement("menuitem");
      document.l10n.setAttributes(m, this._getCapabilityString(state));
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
    return richlistitem;
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

    let hasSelection = this._list.selectedIndex >= 0;
    let hasRows = this._list.itemCount > 0;
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
    p.l10nId = this._getCapabilityString(capability);
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

    if (this._checkbox.checked) {
      Services.prefs.setIntPref(this._defaultPermissionStatePrefName, SitePermissions.BLOCK);
    } else if (this._currentDefaultPermissionsState == SitePermissions.BLOCK) {
      Services.prefs.setIntPref(this._defaultPermissionStatePrefName, SitePermissions.UNKNOWN);
    }

    window.close();
  },

  async buildPermissionsList(sortCol) {
    // Clear old entries.
    let oldItems = this._list.querySelectorAll("richlistitem");
    for (let item of oldItems) {
      item.remove();
    }
    let frag = document.createDocumentFragment();

    let permissions = Array.from(this._permissions.values());

    let keyword = this._searchBox.value.toLowerCase().trim();
    for (let permission of permissions) {
      if (keyword && !permission.origin.includes(keyword)) {
        continue;
      }

      let richlistitem = this._createPermissionListItem(permission);
      frag.appendChild(richlistitem);
    }

    // Sort permissions.
    this._sortPermissions(this._list, frag, sortCol);

    this._list.appendChild(frag);

    this._setRemoveButtonState();
  },

  _sortPermissions(list, frag, column) {
    let sortDirection;

    if (!column) {
      column = document.querySelector("treecol[data-isCurrentSortCol=true]");
      sortDirection = column.getAttribute("data-last-sortDirection") || "ascending";
    } else {
      sortDirection = column.getAttribute("data-last-sortDirection");
      sortDirection = sortDirection === "ascending" ? "descending" : "ascending";
    }

    let sortFunc = null;
    switch (column.id) {
      case "siteCol":
        sortFunc = (a, b) => {
          return comp.compare(a.getAttribute("origin"), b.getAttribute("origin"));
        };
        break;

      case "statusCol":
        sortFunc = (a, b) => {
          return parseInt(a.querySelector("menulist").value) >
            parseInt(b.querySelector("menulist").value);
        };
        break;
    }

    let comp = new Services.intl.Collator(undefined, {
      usage: "sort"
    });

    let items = Array.from(frag.querySelectorAll("richlistitem"));

    if (sortDirection === "descending") {
      items.sort((a, b) => sortFunc(b, a));
    } else {
      items.sort(sortFunc);
    }

    // Re-append items in the correct order:
    items.forEach(item => frag.appendChild(item));

    let cols = list.querySelectorAll("treecol");
    cols.forEach(c => {
      c.removeAttribute("data-isCurrentSortCol");
      c.removeAttribute("sortDirection");
    });
    column.setAttribute("data-isCurrentSortCol", "true");
    column.setAttribute("sortDirection", sortDirection);
    column.setAttribute("data-last-sortDirection", sortDirection);
  },
};
