/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

const nsIPermissionManager = Components.interfaces.nsIPermissionManager;
const nsICookiePermission = Components.interfaces.nsICookiePermission;

const NOTIFICATION_FLUSH_PERMISSIONS = "flush-pending-permissions";

function Permission(principal, type, capability)
{
  this.principal = principal;
  this.origin = principal.origin;
  this.type = type;
  this.capability = capability;
}

var gPermissionManager = {
  _type                 : "",
  _permissions          : [],
  _permissionsToAdd     : new Map(),
  _permissionsToDelete  : new Map(),
  _bundle               : null,
  _tree                 : null,
  _observerRemoved      : false,

  _view: {
    _rowCount: 0,
    get rowCount()
    {
      return this._rowCount;
    },
    getCellText: function (aRow, aColumn)
    {
      if (aColumn.id == "siteCol")
        return gPermissionManager._permissions[aRow].origin;
      else if (aColumn.id == "statusCol")
        return gPermissionManager._permissions[aRow].capability;
      return "";
    },

    isSeparator: function(aIndex) { return false; },
    isSorted: function() { return false; },
    isContainer: function(aIndex) { return false; },
    setTree: function(aTree){},
    getImageSrc: function(aRow, aColumn) {},
    getProgressMode: function(aRow, aColumn) {},
    getCellValue: function(aRow, aColumn) {},
    cycleHeader: function(column) {},
    getRowProperties: function(row){ return ""; },
    getColumnProperties: function(column){ return ""; },
    getCellProperties: function(row,column){
      if (column.element.getAttribute("id") == "siteCol")
        return "ltr";

      return "";
    }
  },

  _getCapabilityString: function (aCapability)
  {
    var stringKey = null;
    switch (aCapability) {
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

  addPermission: function (aCapability)
  {
    var textbox = document.getElementById("url");
    var input_url = textbox.value.replace(/^\s*/, ""); // trim any leading space
    let principal;
    try {
      // If the uri doesn't successfully parse, try adding a http:// and parsing again
      let uri;
      try {
        let uri = Services.io.newURI(input_url, null, null);
      } catch(ex) {
        uri = Services.io.newURI("http://" + input_url, null, null);
      }
      principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(uri);
    } catch(ex) {
      var message = this._bundle.getString("invalidURI");
      var title = this._bundle.getString("invalidURITitle");
      Services.prompt.alert(window, title, message);
      return;
    }

    var capabilityString = this._getCapabilityString(aCapability);

    // check whether the permission already exists, if not, add it
    let permissionExists = false;
    let capabilityExists = false;
    for (var i = 0; i < this._permissions.length; ++i) {
      if (this._permissions[i].principal.equals(principal)) {
        permissionExists = true;
        capabilityExists = this._permissions[i].capability == capabilityString;
        if (!capabilityExists) {
          this._permissions[i].capability = capabilityString;
        }
        break;
      }
    }

    let permissionParams = {principal: principal, type: this._type, capability: aCapability};
    if (!permissionExists) {
      this._permissionsToAdd.set(principal.origin, permissionParams);
      this._addPermission(permissionParams);
    }
    else if (!capabilityExists) {
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

  _removePermission: function(aPermission)
  {
    this._removePermissionFromList(aPermission.principal);

    // If this permission was added during this session, let's remove
    // it from the pending adds list to prevent calls to the
    // permission manager.
    let isNewPermission = this._permissionsToAdd.delete(aPermission.principal.origin);

    if (!isNewPermission) {
      this._permissionsToDelete.set(aPermission.principal.origin, aPermission);
    }

  },

  _handleCapabilityChange: function ()
  {
    // Re-do the sort, if the status changed from Block to Allow
    // or vice versa, since if we're sorted on status, we may no
    // longer be in order.
    if (this._lastPermissionSortColumn == "statusCol") {
      this._resortPermissions();
    }
    this._tree.treeBoxObject.invalidate();
  },

  _addPermission: function(aPermission)
  {
    this._addPermissionToList(aPermission);
    ++this._view._rowCount;
    this._tree.treeBoxObject.rowCountChanged(this._view.rowCount - 1, 1);
    // Re-do the sort, since we inserted this new item at the end.
    this._resortPermissions();
  },

  _resortPermissions: function()
  {
    gTreeUtils.sort(this._tree, this._view, this._permissions,
                    this._lastPermissionSortColumn,
                    this._permissionsComparator,
                    this._lastPermissionSortColumn,
                    !this._lastPermissionSortAscending); // keep sort direction
  },

  onHostInput: function (aSiteField)
  {
    document.getElementById("btnSession").disabled = !aSiteField.value;
    document.getElementById("btnBlock").disabled = !aSiteField.value;
    document.getElementById("btnAllow").disabled = !aSiteField.value;
  },

  onWindowKeyPress: function (aEvent)
  {
    if (aEvent.keyCode == KeyEvent.DOM_VK_ESCAPE)
      window.close();
  },

  onHostKeyPress: function (aEvent)
  {
    if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN)
      document.getElementById("btnAllow").click();
  },

  onLoad: function ()
  {
    this._bundle = document.getElementById("bundlePreferences");
    var params = window.arguments[0];
    this.init(params);
  },

  init: function (aParams)
  {
    if (this._type) {
      // reusing an open dialog, clear the old observer
      this.uninit();
    }

    this._type = aParams.permissionType;
    this._manageCapability = aParams.manageCapability;

    var permissionsText = document.getElementById("permissionsText");
    while (permissionsText.hasChildNodes())
      permissionsText.removeChild(permissionsText.firstChild);
    permissionsText.appendChild(document.createTextNode(aParams.introText));

    document.title = aParams.windowTitle;

    document.getElementById("btnBlock").hidden    = !aParams.blockVisible;
    document.getElementById("btnSession").hidden  = !aParams.sessionVisible;
    document.getElementById("btnAllow").hidden    = !aParams.allowVisible;

    var urlFieldVisible = (aParams.blockVisible || aParams.sessionVisible || aParams.allowVisible);

    var urlField = document.getElementById("url");
    urlField.value = aParams.prefilledHost;
    urlField.hidden = !urlFieldVisible;

    this.onHostInput(urlField);

    var urlLabel = document.getElementById("urlLabel");
    urlLabel.hidden = !urlFieldVisible;

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
    Services.obs.addObserver(this, "perm-changed", false);

    this._loadPermissions();

    urlField.focus();
  },

  uninit: function ()
  {
    if (!this._observerRemoved) {
      Services.obs.removeObserver(this, "perm-changed");

      this._observerRemoved = true;
    }
  },

  observe: function (aSubject, aTopic, aData)
  {
    if (aTopic == "perm-changed") {
      var permission = aSubject.QueryInterface(Components.interfaces.nsIPermission);

      // Ignore unrelated permission types.
      if (permission.type != this._type)
        return;

      if (aData == "added") {
        this._addPermission(permission);
      }
      else if (aData == "changed") {
        for (var i = 0; i < this._permissions.length; ++i) {
          if (permission.matches(this._permissions[i].principal, true)) {
            this._permissions[i].capability = this._getCapabilityString(permission.capability);
            break;
          }
        }
        this._handleCapabilityChange();
      }
      else if (aData == "deleted") {
        this._removePermissionFromList(permission);
      }
    }
  },

  onPermissionSelected: function ()
  {
    var hasSelection = this._tree.view.selection.count > 0;
    var hasRows = this._tree.view.rowCount > 0;
    document.getElementById("removePermission").disabled = !hasRows || !hasSelection;
    document.getElementById("removeAllPermissions").disabled = !hasRows;
  },

  onPermissionDeleted: function ()
  {
    if (!this._view.rowCount)
      return;
    var removedPermissions = [];
    gTreeUtils.deleteSelectedItems(this._tree, this._view, this._permissions, removedPermissions);
    for (var i = 0; i < removedPermissions.length; ++i) {
      var p = removedPermissions[i];
      this._removePermission(p);
    }
    document.getElementById("removePermission").disabled = !this._permissions.length;
    document.getElementById("removeAllPermissions").disabled = !this._permissions.length;
  },

  onAllPermissionsDeleted: function ()
  {
    if (!this._view.rowCount)
      return;
    var removedPermissions = [];
    gTreeUtils.deleteAll(this._tree, this._view, this._permissions, removedPermissions);
    for (var i = 0; i < removedPermissions.length; ++i) {
      var p = removedPermissions[i];
      this._removePermission(p);
    }
    document.getElementById("removePermission").disabled = true;
    document.getElementById("removeAllPermissions").disabled = true;
  },

  onPermissionKeyPress: function (aEvent)
  {
    if (aEvent.keyCode == KeyEvent.DOM_VK_DELETE
#ifdef XP_MACOSX
        || aEvent.keyCode == KeyEvent.DOM_VK_BACK_SPACE
#endif
       )
      this.onPermissionDeleted();
  },

  _lastPermissionSortColumn: "",
  _lastPermissionSortAscending: false,
  _permissionsComparator : function (a, b)
  {
    return a.toLowerCase().localeCompare(b.toLowerCase());
  },


  onPermissionSort: function (aColumn)
  {
    this._lastPermissionSortAscending = gTreeUtils.sort(this._tree,
                                                        this._view,
                                                        this._permissions,
                                                        aColumn,
                                                        this._permissionsComparator,
                                                        this._lastPermissionSortColumn,
                                                        this._lastPermissionSortAscending);
    this._lastPermissionSortColumn = aColumn;
  },

  onApplyChanges: function()
  {
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

  _loadPermissions: function ()
  {
    this._tree = document.getElementById("permissionsTree");
    this._permissions = [];

    // load permissions into a table
    var count = 0;
    var enumerator = Services.perms.enumerator;
    while (enumerator.hasMoreElements()) {
      var nextPermission = enumerator.getNext().QueryInterface(Components.interfaces.nsIPermission);
      this._addPermissionToList(nextPermission);
    }

    this._view._rowCount = this._permissions.length;

    // sort and display the table
    this._tree.view = this._view;
    this.onPermissionSort("origin");

    // disable "remove all" button if there are none
    document.getElementById("removeAllPermissions").disabled = this._permissions.length == 0;
  },

  _addPermissionToList: function (aPermission)
  {
    if (aPermission.type == this._type &&
        (!this._manageCapability ||
         (aPermission.capability == this._manageCapability))) {

      var principal = aPermission.principal;
      var capabilityString = this._getCapabilityString(aPermission.capability);
      var p = new Permission(principal,
                             aPermission.type,
                             capabilityString);
      this._permissions.push(p);
    }
  },

  _removePermissionFromList: function (aPrincipal)
  {
    for (let i = 0; i < this._permissions.length; ++i) {
      if (this._permissions[i].principal.equals(aPrincipal)) {
        this._permissions.splice(i, 1);
        this._view._rowCount--;
        this._tree.treeBoxObject.rowCountChanged(this._view.rowCount - 1, -1);
        this._tree.treeBoxObject.invalidate();
        break;
      }
    }
  },

  setOrigin: function (aOrigin)
  {
    document.getElementById("url").value = aOrigin;
  }
};

function setOrigin(aOrigin)
{
  gPermissionManager.setOrigin(aOrigin);
}

function initWithParams(aParams)
{
  gPermissionManager.init(aParams);
}
