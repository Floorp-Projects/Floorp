/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Imported via permissions.xul.
/* import-globals-from ../../../toolkit/content/treeUtils.js */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const nsIPermissionManager = Ci.nsIPermissionManager;
const nsICookiePermission = Ci.nsICookiePermission;

const NOTIFICATION_FLUSH_PERMISSIONS = "flush-pending-permissions";

const permissionExceptionsL10n = {
  "trackingprotection": {
    window: "permissions-exceptions-tracking-protection-window",
    description: "permissions-exceptions-tracking-protection-desc",
  },
  "cookie": {
    window: "permissions-exceptions-cookie-window",
    description: "permissions-exceptions-cookie-desc",
  },
  "popup": {
    window: "permissions-exceptions-popup-window",
    description: "permissions-exceptions-popup-desc",
  },
  "login-saving": {
    window: "permissions-exceptions-saved-logins-window",
    description: "permissions-exceptions-saved-logins-desc",
  },
  "install": {
    window: "permissions-exceptions-addons-window",
    description: "permissions-exceptions-addons-desc",
  },
  "autoplay-media": {
    window: "permissions-exceptions-autoplay-media-window",
    description: "permissions-exceptions-autoplay-media-desc",
  },
};

function Permission(principal, type, capability) {
  this.principal = principal;
  this.origin = principal.origin;
  this.type = type;
  this.capability = capability;
}

var gPermissionManager = {
  _type: "",
  _permissions: [],
  _permissionsToAdd: new Map(),
  _permissionsToDelete: new Map(),
  _bundle: null,
  _tree: null,
  _observerRemoved: false,

  _view: {
    _rowCount: 0,
    get rowCount() {
      return this._rowCount;
    },
    getCellText(row, column) {
      if (column.id == "siteCol")
        return gPermissionManager._permissions[row].origin;
      else if (column.id == "statusCol")
        return gPermissionManager._permissions[row].capability;
      return "";
    },

    isSeparator(index) { return false; },
    isSorted() { return false; },
    isContainer(index) { return false; },
    setTree(tree) {},
    getImageSrc(row, column) {},
    getCellValue(row, column) {},
    cycleHeader(column) {},
    getRowProperties(row) { return ""; },
    getColumnProperties(column) { return ""; },
    getCellProperties(row, column) {
      if (column.element.getAttribute("id") == "siteCol")
        return "ltr";

      return "";
    }
  },

  onLoad() {
    this._bundle = document.getElementById("bundlePreferences");
    let params = window.arguments[0];
    document.mozSubdialogReady = this.init(params);
  },

  async init(params) {
    if (this._type) {
      // reusing an open dialog, clear the old observer
      this.uninit();
    }

    this._type = params.permissionType;
    this._manageCapability = params.manageCapability;

    const l10n = permissionExceptionsL10n[this._type];
    let permissionsText = document.getElementById("permissionsText");
    document.l10n.setAttributes(permissionsText, l10n.description);

    document.l10n.setAttributes(document.documentElement, l10n.window);

    await document.l10n.translateElements([
      document.documentElement,
      permissionsText,
    ]);

    document.getElementById("btnBlock").hidden    = !params.blockVisible;
    document.getElementById("btnSession").hidden  = !params.sessionVisible;
    document.getElementById("btnAllow").hidden    = !params.allowVisible;

    let urlFieldVisible = (params.blockVisible || params.sessionVisible || params.allowVisible);

    let urlField = document.getElementById("url");
    urlField.value = params.prefilledHost;
    urlField.hidden = !urlFieldVisible;

    this.onHostInput(urlField);

    let urlLabel = document.getElementById("urlLabel");
    urlLabel.hidden = !urlFieldVisible;

    if (params.hideStatusColumn) {
      document.getElementById("statusCol").hidden = true;
    }

    let treecols = document.getElementsByTagName("treecols")[0];
    treecols.addEventListener("click", event => {
      if (event.target.nodeName != "treecol" || event.button != 0) {
        return;
      }

      let sortField = event.target.getAttribute("data-field-name");
      if (!sortField) {
        return;
      }

      gPermissionManager.onPermissionSort(sortField);
    });

    Services.obs.notifyObservers(null, NOTIFICATION_FLUSH_PERMISSIONS, this._type);
    Services.obs.addObserver(this, "perm-changed");

    this._loadPermissions();

    urlField.focus();
  },

  uninit() {
    if (!this._observerRemoved) {
      Services.obs.removeObserver(this, "perm-changed");

      this._observerRemoved = true;
    }
  },

  observe(subject, topic, data) {
    if (topic == "perm-changed") {
      let permission = subject.QueryInterface(Ci.nsIPermission);

      // Ignore unrelated permission types.
      if (permission.type != this._type)
        return;

      if (data == "added") {
        this._addPermission(permission);
      } else if (data == "changed") {
        for (let i = 0; i < this._permissions.length; ++i) {
          if (permission.matches(this._permissions[i].principal, true)) {
            this._permissions[i].capability = this._getCapabilityString(permission.capability);
            break;
          }
        }
        this._handleCapabilityChange();
      } else if (data == "deleted") {
        this._removePermissionFromList(permission.principal);
      }
    }
  },

  _resortPermissions() {
    gTreeUtils.sort(this._tree, this._view, this._permissions,
                    this._lastPermissionSortColumn,
                    this._permissionsComparator,
                    this._lastPermissionSortColumn,
                    !this._lastPermissionSortAscending); // keep sort direction
  },

  _handleCapabilityChange() {
    // Re-do the sort, if the status changed from Block to Allow
    // or vice versa, since if we're sorted on status, we may no
    // longer be in order.
    if (this._lastPermissionSortColumn == "statusCol") {
      this._resortPermissions();
    }
    this._tree.treeBoxObject.invalidate();
  },

  _getCapabilityString(capability) {
    let stringKey = null;
    switch (capability) {
    case nsIPermissionManager.ALLOW_ACTION:
      stringKey = "can";
      break;
    case nsIPermissionManager.DENY_ACTION:
      stringKey = "cannot";
      break;
    case nsICookiePermission.ACCESS_ALLOW_FIRST_PARTY_ONLY:
      stringKey = "canAccessFirstParty";
      break;
    case nsICookiePermission.ACCESS_SESSION:
      stringKey = "canSession";
      break;
    }
    return this._bundle.getString(stringKey);
  },

  _addPermission(permission) {
    this._addPermissionToList(permission);
    ++this._view._rowCount;
    this._tree.treeBoxObject.rowCountChanged(this._view.rowCount - 1, 1);
    // Re-do the sort, since we inserted this new item at the end.
    this._resortPermissions();
  },

  _addPermissionToList(permission) {
    if (permission.type == this._type &&
        (!this._manageCapability ||
         (permission.capability == this._manageCapability))) {

      let principal = permission.principal;
      let capabilityString = this._getCapabilityString(permission.capability);
      let p = new Permission(principal,
                             permission.type,
                             capabilityString);
      this._permissions.push(p);
    }
  },

  addPermission(capability) {
    let textbox = document.getElementById("url");
    let input_url = textbox.value.replace(/^\s*/, ""); // trim any leading space
    let principal;
    try {
      // The origin accessor on the principal object will throw if the
      // principal doesn't have a canonical origin representation. This will
      // help catch cases where the URI parser parsed something like
      // `localhost:8080` as having the scheme `localhost`, rather than being
      // an invalid URI. A canonical origin representation is required by the
      // permission manager for storage, so this won't prevent any valid
      // permissions from being entered by the user.
      let uri;
      try {
        uri = Services.io.newURI(input_url);
        principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
        if (principal.origin.startsWith("moz-nullprincipal:")) {
          throw "Null principal";
        }
      } catch (ex) {
        uri = Services.io.newURI("http://" + input_url);
        principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
        // If we have ended up with an unknown scheme, the following will throw.
        principal.origin;
      }
    } catch (ex) {
      document.l10n.formatValues([
        {id: "permissions-invalid-uri-title"},
        {id: "permissions-invalid-uri-label"}
      ]).then(([title, message]) => {
        Services.prompt.alert(window, title, message);
      });
      return;
    }

    let capabilityString = this._getCapabilityString(capability);

    // check whether the permission already exists, if not, add it
    let permissionExists = false;
    let capabilityExists = false;
    for (let i = 0; i < this._permissions.length; ++i) {
      if (this._permissions[i].principal.equals(principal)) {
        permissionExists = true;
        capabilityExists = this._permissions[i].capability == capabilityString;
        if (!capabilityExists) {
          this._permissions[i].capability = capabilityString;
        }
        break;
      }
    }

    let permissionParams = {principal, type: this._type, capability};
    if (!permissionExists) {
      this._permissionsToAdd.set(principal.origin, permissionParams);
      this._addPermission(permissionParams);
    } else if (!capabilityExists) {
      this._permissionsToAdd.set(principal.origin, permissionParams);
      this._handleCapabilityChange();
    }

    textbox.value = "";
    textbox.focus();

    // covers a case where the site exists already, so the buttons don't disable
    this.onHostInput(textbox);

    // enable "remove all" button as needed
    document.getElementById("removeAllPermissions").disabled = this._permissions.length == 0;
  },

  _removePermission(permission) {
    this._removePermissionFromList(permission.principal);

    // If this permission was added during this session, let's remove
    // it from the pending adds list to prevent calls to the
    // permission manager.
    let isNewPermission = this._permissionsToAdd.delete(permission.principal.origin);

    if (!isNewPermission) {
      this._permissionsToDelete.set(permission.principal.origin, permission);
    }

  },

  _removePermissionFromList(principal) {
    for (let i = 0; i < this._permissions.length; ++i) {
      if (this._permissions[i].principal.equals(principal)) {
        this._permissions.splice(i, 1);
        this._view._rowCount--;
        this._tree.treeBoxObject.rowCountChanged(this._view.rowCount - 1, -1);
        this._tree.treeBoxObject.invalidate();
        break;
      }
    }
  },

  _loadPermissions() {
    this._tree = document.getElementById("permissionsTree");
    this._permissions = [];

    // load permissions into a table
    let enumerator = Services.perms.enumerator;
    while (enumerator.hasMoreElements()) {
      let nextPermission = enumerator.getNext().QueryInterface(Ci.nsIPermission);
      this._addPermissionToList(nextPermission);
    }

    this._view._rowCount = this._permissions.length;

    // sort and display the table
    this._tree.view = this._view;
    this.onPermissionSort("origin");

    // disable "remove all" button if there are none
    document.getElementById("removeAllPermissions").disabled = this._permissions.length == 0;
  },

  onWindowKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_ESCAPE)
      window.close();
  },

  onPermissionKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_DELETE) {
      this.onPermissionDelete();
    } else if (AppConstants.platform == "macosx" &&
               event.keyCode == KeyEvent.DOM_VK_BACK_SPACE) {
      this.onPermissionDelete();
      event.preventDefault();
    }
  },

  onHostKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_RETURN)
      document.getElementById("btnAllow").click();
  },

  onHostInput(siteField) {
    document.getElementById("btnSession").disabled = !siteField.value;
    document.getElementById("btnBlock").disabled = !siteField.value;
    document.getElementById("btnAllow").disabled = !siteField.value;
  },

  onPermissionDelete() {
    if (!this._view.rowCount)
      return;
    let removedPermissions = [];
    gTreeUtils.deleteSelectedItems(this._tree, this._view, this._permissions, removedPermissions);
    for (let i = 0; i < removedPermissions.length; ++i) {
      let p = removedPermissions[i];
      this._removePermission(p);
    }
    document.getElementById("removePermission").disabled = !this._permissions.length;
    document.getElementById("removeAllPermissions").disabled = !this._permissions.length;
  },

  onAllPermissionsDelete() {
    if (!this._view.rowCount)
      return;
    let removedPermissions = [];
    gTreeUtils.deleteAll(this._tree, this._view, this._permissions, removedPermissions);
    for (let i = 0; i < removedPermissions.length; ++i) {
      let p = removedPermissions[i];
      this._removePermission(p);
    }
    document.getElementById("removePermission").disabled = true;
    document.getElementById("removeAllPermissions").disabled = true;
  },

  onPermissionSelect() {
    let hasSelection = this._tree.view.selection.count > 0;
    let hasRows = this._tree.view.rowCount > 0;
    document.getElementById("removePermission").disabled = !hasRows || !hasSelection;
    document.getElementById("removeAllPermissions").disabled = !hasRows;
  },

  onApplyChanges() {
    // Stop observing permission changes since we are about
    // to write out the pending adds/deletes and don't need
    // to update the UI
    this.uninit();

    for (let permissionParams of this._permissionsToAdd.values()) {
      Services.perms.addFromPrincipal(permissionParams.principal, permissionParams.type, permissionParams.capability);
    }

    for (let p of this._permissionsToDelete.values()) {
      Services.perms.removeFromPrincipal(p.principal, p.type);
    }

    window.close();
  },

  _lastPermissionSortColumn: "",
  _lastPermissionSortAscending: false,
  _permissionsComparator(a, b) {
    return a.toLowerCase().localeCompare(b.toLowerCase());
  },

  onPermissionSort(column) {
    this._lastPermissionSortAscending = gTreeUtils.sort(this._tree,
                                                        this._view,
                                                        this._permissions,
                                                        column,
                                                        this._permissionsComparator,
                                                        this._lastPermissionSortColumn,
                                                        this._lastPermissionSortAscending);
    this._lastPermissionSortColumn = column;
    let sortDirection = this._lastPermissionSortAscending ? "descending" : "ascending";
    let cols = document.querySelectorAll("treecol");
    cols.forEach(c => c.removeAttribute("sortDirection"));
    document.querySelector(`treecol[data-field-name=${column}]`)
            .setAttribute("sortDirection", sortDirection);
  },
};

function initWithParams(params) {
  gPermissionManager.init(params);
}
