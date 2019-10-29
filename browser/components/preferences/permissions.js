/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const permissionExceptionsL10n = {
  trackingprotection: {
    window: "permissions-exceptions-etp-window",
    description: "permissions-exceptions-etp-desc",
  },
  cookie: {
    window: "permissions-exceptions-cookie-window",
    description: "permissions-exceptions-cookie-desc",
  },
  popup: {
    window: "permissions-exceptions-popup-window",
    description: "permissions-exceptions-popup-desc",
  },
  "login-saving": {
    window: "permissions-exceptions-saved-logins-window",
    description: "permissions-exceptions-saved-logins-desc",
  },
  install: {
    window: "permissions-exceptions-addons-window",
    description: "permissions-exceptions-addons-desc",
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
  _isObserving: false,
  _permissions: new Map(),
  _permissionsToAdd: new Map(),
  _permissionsToDelete: new Map(),
  _bundle: null,
  _list: null,
  _removeButton: null,
  _removeAllButton: null,

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

    let permissionsText = document.getElementById("permissionsText");

    let l10n = permissionExceptionsL10n[this._type];
    document.l10n.setAttributes(permissionsText, l10n.description);
    document.l10n.setAttributes(document.documentElement, l10n.window);

    await document.l10n.translateElements([
      permissionsText,
      document.documentElement,
    ]);

    document.getElementById("btnBlock").hidden = !params.blockVisible;
    document.getElementById("btnSession").hidden = !params.sessionVisible;
    document.getElementById("btnAllow").hidden = !params.allowVisible;

    let urlFieldVisible =
      params.blockVisible || params.sessionVisible || params.allowVisible;

    let urlField = document.getElementById("url");
    urlField.value = params.prefilledHost;
    urlField.hidden = !urlFieldVisible;

    this.onHostInput(urlField);

    let urlLabel = document.getElementById("urlLabel");
    urlLabel.hidden = !urlFieldVisible;

    this._hideStatusColumn = params.hideStatusColumn;
    let statusCol = document.getElementById("statusCol");
    statusCol.hidden = this._hideStatusColumn;
    if (this._hideStatusColumn) {
      statusCol.removeAttribute("data-isCurrentSortCol");
      document
        .getElementById("siteCol")
        .setAttribute("data-isCurrentSortCol", "true");
    }

    Services.obs.notifyObservers(null, "flush-pending-permissions", this._type);

    this._loadPermissions();
    this.buildPermissionsList();

    urlField.focus();
  },

  uninit() {
    if (this._isObserving) {
      Services.obs.removeObserver(this, "perm-changed");
      this._isObserving = false;
    }
  },

  observe(subject, topic, data) {
    if (topic !== "perm-changed") {
      return;
    }

    let permission = subject.QueryInterface(Ci.nsIPermission);

    // Ignore unrelated permission types.
    if (permission.type !== this._type) {
      return;
    }

    if (data == "added") {
      this._addPermissionToList(permission);
      this.buildPermissionsList();
    } else if (data == "changed") {
      let p = this._permissions.get(permission.principal.origin);
      // Maybe this item has been excluded before because it had an invalid capability.
      if (p) {
        p.capability = permission.capability;
        this._handleCapabilityChange(p);
      } else {
        this._addPermissionToList(permission);
      }
      this.buildPermissionsList();
    } else if (data == "deleted") {
      this._removePermissionFromList(permission.principal.origin);
    }
  },

  _handleCapabilityChange(perm) {
    let permissionlistitem = document.getElementsByAttribute(
      "origin",
      perm.origin
    )[0];
    document.l10n.setAttributes(
      permissionlistitem.querySelector(".website-capability-value"),
      this._getCapabilityL10nId(perm.capability)
    );
  },

  _isCapabilitySupported(capability) {
    return (
      capability == Ci.nsIPermissionManager.ALLOW_ACTION ||
      capability == Ci.nsIPermissionManager.DENY_ACTION ||
      capability == Ci.nsICookiePermission.ACCESS_SESSION
    );
  },

  _getCapabilityL10nId(capability) {
    let stringKey = null;
    switch (capability) {
      case Ci.nsIPermissionManager.ALLOW_ACTION:
        stringKey = "permissions-capabilities-listitem-allow";
        break;
      case Ci.nsIPermissionManager.DENY_ACTION:
        stringKey = "permissions-capabilities-listitem-block";
        break;
      case Ci.nsICookiePermission.ACCESS_SESSION:
        stringKey = "permissions-capabilities-listitem-allow-session";
        break;
      default:
        throw new Error(`Unknown capability: ${capability}`);
    }
    return stringKey;
  },

  _addPermissionToList(perm) {
    if (perm.type !== this._type) {
      return;
    }
    if (!this._isCapabilitySupported(perm.capability)) {
      return;
    }

    let p = new Permission(perm.principal, perm.type, perm.capability);
    this._permissions.set(p.origin, p);
  },

  _addOrModifyPermission(principal, capability) {
    // check whether the permission already exists, if not, add it
    let permissionParams = { principal, type: this._type, capability };
    let existingPermission = this._permissions.get(principal.origin);
    if (!existingPermission) {
      this._permissionsToAdd.set(principal.origin, permissionParams);
      this._addPermissionToList(permissionParams);
      this.buildPermissionsList();
    } else if (existingPermission.capability != capability) {
      existingPermission.capability = capability;
      this._permissionsToAdd.set(principal.origin, permissionParams);
      this._handleCapabilityChange(existingPermission);
    }
  },

  _addNewPrincipalToList(list, uri) {
    list.push(Services.scriptSecurityManager.createContentPrincipal(uri, {}));
    // If we have ended up with an unknown scheme, the following will throw.
    list[list.length - 1].origin;
  },

  addPermission(capability) {
    let textbox = document.getElementById("url");
    let input_url = textbox.value.trim(); // trim any leading and trailing space
    let principals = [];
    try {
      // The origin accessor on the principal object will throw if the
      // principal doesn't have a canonical origin representation. This will
      // help catch cases where the URI parser parsed something like
      // `localhost:8080` as having the scheme `localhost`, rather than being
      // an invalid URI. A canonical origin representation is required by the
      // permission manager for storage, so this won't prevent any valid
      // permissions from being entered by the user.
      try {
        let uri = Services.io.newURI(input_url);
        let principal = Services.scriptSecurityManager.createContentPrincipal(
          uri,
          {}
        );
        if (principal.origin.startsWith("moz-nullprincipal:")) {
          throw new Error("Null principal");
        }
        principals.push(principal);
      } catch (ex) {
        this._addNewPrincipalToList(
          principals,
          Services.io.newURI("http://" + input_url)
        );
        this._addNewPrincipalToList(
          principals,
          Services.io.newURI("https://" + input_url)
        );
      }
    } catch (ex) {
      document.l10n
        .formatValues([
          { id: "permissions-invalid-uri-title" },
          { id: "permissions-invalid-uri-label" },
        ])
        .then(([title, message]) => {
          Services.prompt.alert(window, title, message);
        });
      return;
    }

    for (let principal of principals) {
      this._addOrModifyPermission(principal, capability);
    }

    textbox.value = "";
    textbox.focus();

    // covers a case where the site exists already, so the buttons don't disable
    this.onHostInput(textbox);

    // enable "remove all" button as needed
    this._setRemoveButtonState();
  },

  _removePermission(permission) {
    this._removePermissionFromList(permission.origin);

    // If this permission was added during this session, let's remove
    // it from the pending adds list to prevent calls to the
    // permission manager.
    let isNewPermission = this._permissionsToAdd.delete(permission.origin);
    if (!isNewPermission) {
      this._permissionsToDelete.set(permission.origin, permission);
    }
  },

  _removePermissionFromList(origin) {
    this._permissions.delete(origin);
    let permissionlistitem = document.getElementsByAttribute(
      "origin",
      origin
    )[0];
    if (permissionlistitem) {
      permissionlistitem.remove();
    }
  },

  _loadPermissions() {
    // load permissions into a table.
    for (let nextPermission of Services.perms.enumerator) {
      this._addPermissionToList(nextPermission);
    }
  },

  _createPermissionListItem(permission) {
    let richlistitem = document.createXULElement("richlistitem");
    richlistitem.setAttribute("origin", permission.origin);
    let row = document.createXULElement("hbox");
    row.setAttribute("flex", "1");

    let hbox = document.createXULElement("hbox");
    let website = document.createXULElement("label");
    website.setAttribute("value", permission.origin);
    hbox.setAttribute("width", "0");
    hbox.setAttribute("class", "website-name");
    hbox.setAttribute("flex", "3");
    hbox.appendChild(website);
    row.appendChild(hbox);

    if (!this._hideStatusColumn) {
      hbox = document.createXULElement("hbox");
      let capability = document.createXULElement("label");
      capability.setAttribute("class", "website-capability-value");
      document.l10n.setAttributes(
        capability,
        this._getCapabilityL10nId(permission.capability)
      );
      hbox.setAttribute("width", "0");
      hbox.setAttribute("class", "website-name");
      hbox.setAttribute("flex", "1");
      hbox.appendChild(capability);
      row.appendChild(hbox);
    }

    richlistitem.appendChild(row);
    return richlistitem;
  },

  onWindowKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_ESCAPE) {
      window.close();
    }
  },

  onPermissionKeyPress(event) {
    if (!this._list.selectedItem) {
      return;
    }

    if (
      event.keyCode == KeyEvent.DOM_VK_DELETE ||
      (AppConstants.platform == "macosx" &&
        event.keyCode == KeyEvent.DOM_VK_BACK_SPACE)
    ) {
      this.onPermissionDelete();
      event.preventDefault();
    }
  },

  onHostKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_RETURN) {
      document.getElementById("btnAllow").click();
    }
  },

  onHostInput(siteField) {
    document.getElementById("btnSession").disabled = !siteField.value;
    document.getElementById("btnBlock").disabled = !siteField.value;
    document.getElementById("btnAllow").disabled = !siteField.value;
  },

  _setRemoveButtonState() {
    if (!this._list) {
      return;
    }

    let hasSelection = this._list.selectedIndex >= 0;
    let hasRows = this._list.itemCount > 0;
    this._removeButton.disabled = !hasSelection;
    this._removeAllButton.disabled = !hasRows;
  },

  onPermissionDelete() {
    let richlistitem = this._list.selectedItem;
    let origin = richlistitem.getAttribute("origin");
    let permission = this._permissions.get(origin);

    this._removePermission(permission);

    this._setRemoveButtonState();
  },

  onAllPermissionsDelete() {
    for (let permission of this._permissions.values()) {
      this._removePermission(permission);
    }

    this._setRemoveButtonState();
  },

  onPermissionSelect() {
    this._setRemoveButtonState();
  },

  onApplyChanges() {
    // Stop observing permission changes since we are about
    // to write out the pending adds/deletes and don't need
    // to update the UI
    this.uninit();

    for (let p of this._permissionsToDelete.values()) {
      Services.perms.removeFromPrincipal(p.principal, p.type);
    }

    for (let p of this._permissionsToAdd.values()) {
      Services.perms.addFromPrincipal(p.principal, p.type, p.capability);
    }

    window.close();
  },

  buildPermissionsList(sortCol) {
    // Clear old entries.
    let oldItems = this._list.querySelectorAll("richlistitem");
    for (let item of oldItems) {
      item.remove();
    }
    let frag = document.createDocumentFragment();

    let permissions = Array.from(this._permissions.values());

    for (let permission of permissions) {
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
      sortDirection =
        column.getAttribute("data-last-sortDirection") || "ascending";
    } else {
      sortDirection = column.getAttribute("data-last-sortDirection");
      sortDirection =
        sortDirection === "ascending" ? "descending" : "ascending";
    }

    let sortFunc = null;
    switch (column.id) {
      case "siteCol":
        sortFunc = (a, b) => {
          return comp.compare(
            a.getAttribute("origin"),
            b.getAttribute("origin")
          );
        };
        break;

      case "statusCol":
        sortFunc = (a, b) => {
          // The capabilities values ("Allow" and "Block") are localized asynchronously.
          // Sort based on the guaranteed-present localization ID instead, note that the
          // ascending/descending arrow may be pointing the wrong way.
          return (
            a
              .querySelector(".website-capability-value")
              .getAttribute("data-l10n-id") >
            b
              .querySelector(".website-capability-value")
              .getAttribute("data-l10n-id")
          );
        };
        break;
    }

    let comp = new Services.intl.Collator(undefined, {
      usage: "sort",
    });

    let items = Array.from(frag.querySelectorAll("richlistitem"));

    if (sortDirection === "descending") {
      items.sort((a, b) => sortFunc(b, a));
    } else {
      items.sort(sortFunc);
    }

    // Re-append items in the correct order:
    items.forEach(item => frag.appendChild(item));

    let cols = list.previousElementSibling.querySelectorAll("treecol");
    cols.forEach(c => {
      c.removeAttribute("data-isCurrentSortCol");
      c.removeAttribute("sortDirection");
    });
    column.setAttribute("data-isCurrentSortCol", "true");
    column.setAttribute("sortDirection", sortDirection);
    column.setAttribute("data-last-sortDirection", sortDirection);
  },
};
