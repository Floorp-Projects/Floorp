# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Blake Ross <firefox@blakeross.com>
#   Doron Rosenberg <doronr@us.ibm.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

const nsIPermissionManager = Components.interfaces.nsIPermissionManager;

function Permission(host, rawHost, type, capability, perm)
{
  this.host = host;
  this.rawHost = rawHost;
  this.type = type;
  this.capability = capability;
  this.perm = perm;
}

var gPermissionManager = {
  _type         : "",
  _permissions  : [],
  _pm           : Components.classes["@mozilla.org/permissionmanager;1"]
                            .getService(Components.interfaces.nsIPermissionManager),
  _bundle       : null,
  _xformsBundle : null,
  _tree         : null,
  _loaded       : false,

  _view: {
    _rowCount: 0,
    get rowCount()
    {
      return this._rowCount;
    },
    getCellText: function (aRow, aColumn)
    {
      if (aColumn.id == "siteCol")
        return gPermissionManager._permissions[aRow].rawHost;
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
    getRowProperties: function(row,prop){},
    getColumnProperties: function(column,prop){},
    getCellProperties: function(row,column,prop){}
  },

  _getCapabilityString: function (aCapability)
  {
    var stringKey = null;
    switch (aCapability) {
    case 1:
      stringKey = "xformsXDPermissionSend";
      break;
    case 2:
      stringKey = "xformsXDPermissionLoad";
      break;
    case 3:
      stringKey = "xformsXDPermissionLoadSend";
      break;
    }

    return this._xformsBundle.getString(stringKey);
  },

  addPermission: function (aIsSave)
  {
    var textbox = document.getElementById("url");
    var host = textbox.value.replace(/^\s*([-\w]*:\/+)?/, ""); // trim any leading space and scheme
    try {
      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(Components.interfaces.nsIIOService);
      var uri = ioService.newURI("http://"+host, null, null);
      host = uri.host;
    } catch(ex) {
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                    .getService(Components.interfaces.nsIPromptService);
      var message = this._bundle.getString("invalidURI");
      var title = this._bundle.getString("invalidURITitle");
      promptService.alert(window, title, message);
      return;
    }

    var permission = 0;
    var canLoad = document.getElementById("checkLoad").checked;
    var canSend = document.getElementById("checkSend").checked;

    if (!canLoad && !canSend) {
      // invalid
      return;
    } else if (canLoad && canSend) {
      permission = 3;
    } else {
      permission = canSend ? 1 : 2;
    }

    // check whether the permission already exists, if not, add it
    var exists = false;
    for (var i = 0; i < this._permissions.length; ++i) {
      // check if the host and the permission matches
      if (this._permissions[i].rawHost == host) {
        if (this._permissions[i].perm == permission) {
          exists = true;
        }

        // update the array entry
        var capabilityString = this._getCapabilityString(permission);
        this._permissions[i].capability = capabilityString;
        this._permissions[i].perm = permission;

        break;
      }
    }

    if (!exists) {
      if (aIsSave) {
        // if it doesn't exist, but we were saving, remove the entry we were
        // editing.
        var index = this._tree.currentIndex;
        var oldurl = this._tree.view.getCellText(index, this._tree.columns.getFirstColumn());

        if (oldurl != host) {
          this.onPermissionDeleted();
        }
      }

      host = (host.charAt(0) == ".") ? host.substring(1,host.length) : host;
      var uri = ioService.newURI("http://" + host, null, null);
      this._pm.add(uri, this._type, permission);
    }

    // clear the checkboxes
    document.getElementById("checkLoad").checked = false;
    document.getElementById("checkSend").checked = false;

    this.clear();
    textbox.focus();

    // covers a case where the site exists already, so the buttons don't disable
    this.onHostInput(textbox);

    // enable "remove all" button as needed
    document.getElementById("removeAllPermissions").disabled = this._permissions.length == 0;
  },

  onHostInput: function ()
  {
    // if no checkbox selected, disable button.
    if (!document.getElementById("checkLoad").checked &&
        !document.getElementById("checkSend").checked) {
      document.getElementById("btnAdd").disabled = true;
      document.getElementById("btnSave").disabled = true;
    } else {
      var input = document.getElementById("url");
      document.getElementById("btnAdd").disabled = !input.value;
      document.getElementById("btnSave").disabled = !input.value;
    }
  },

  onHostKeyPress: function (aEvent)
  {
    if (aEvent.keyCode == 13)
      gPermissionManager.addPermission();
  },

  onLoad: function ()
  {
    this._bundle = document.getElementById("bundlePreferences");
    this._xformsBundle = document.getElementById("xformsBundle");

    var params = window.arguments[0];
    this.init(params);

    // clear selection
    this._tree.view.selection.clearSelection();

    this._loaded = true;
  },

  init: function (aParams)
  {
    this._type = aParams.permissionType;

    var permissionsText = document.getElementById("permissionsText");
    while (permissionsText.hasChildNodes())
      permissionsText.removeChild(permissionsText.firstChild);
    permissionsText.appendChild(document.createTextNode(aParams.introText));

    document.title = aParams.windowTitle;

    var urlField = document.getElementById("url");
    this.onHostInput(urlField);

    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "perm-changed", false);

    this._loadPermissions();

    urlField.focus();
  },

  uninit: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.removeObserver(this, "perm-changed");
  },

  observe: function (aSubject, aTopic, aData)
  {
    if (aTopic == "perm-changed") {
      var permission = aSubject.QueryInterface(Components.interfaces.nsIPermission);
      if (aData == "added") {
        this._addPermissionToList(permission);
        ++this._view._rowCount;
        this._tree.treeBoxObject.rowCountChanged(this._view.rowCount - 1, 1);
        // Re-do the sort, since we inserted this new item at the end.
        gTreeUtils.sort(this._tree, this._view, this._permissions,
                        this._lastPermissionSortColumn,
                        this._lastPermissionSortAscending); 
      }
      else if (aData == "changed") {
        for (var i = 0; i < this._permissions.length; ++i) {
          if (this._permissions[i].host == permission.host) {
            this._permissions[i].capability = this._getCapabilityString(permission.capability);
            break;
          }
        }
        // Re-do the sort, if the status changed from Block to Allow
        // or vice versa, since if we're sorted on status, we may no
        // longer be in order. 
        if (this._lastPermissionSortColumn.id == "statusCol") {
          gTreeUtils.sort(this._tree, this._view, this._permissions, 
                          this._lastPermissionSortColumn, 
                          this._lastPermissionSortAscending);
        }
        this._tree.treeBoxObject.invalidate();
      }
      // No UI other than this window causes this method to be sent a "deleted"
      // notification, so we don't need to implement it since Delete is handled
      // directly by the Permission Removal handlers. If that ever changes, those
      // implementations will have to move into here.
    }
  },

  onPermissionSelected: function ()
  {
    var hasSelection = this._tree.view.selection.count > 0;
    var hasRows = this._tree.view.rowCount > 0;
    document.getElementById("removePermission").disabled = !hasRows || !hasSelection;
    document.getElementById("removeAllPermissions").disabled = !hasRows;

    if (this._loaded && hasSelection && hasRows) {
      document.getElementById("btnAdd").hidden = true;
      document.getElementById("btnSave").hidden = false;
      document.getElementById("btnSave").disabled = false;

      var index = this._tree.currentIndex;
      if (index >= 0) {
        var url = this._tree.view.getCellText(index,
                                              this._tree.columns.getFirstColumn());
        document.getElementById("url").value = url;

        // look for the permission
        for (var i = 0; i < this._permissions.length; ++i) {
          if (this._permissions[i].rawHost == url) {
            var perm = this._permissions[i].perm;

            document.getElementById("checkLoad").checked = !(perm == 1);
            document.getElementById("checkSend").checked = !(perm == 2);
            break;
          }
        }
      }
    }
  },

  onPermissionDeleted: function ()
  {
    if (!this._view.rowCount)
      return;
    var removedPermissions = [];
    gTreeUtils.deleteSelectedItems(this._tree, this._view, this._permissions, removedPermissions);
    for (var i = 0; i < removedPermissions.length; ++i) {
      var p = removedPermissions[i];
      this._pm.remove(p.host, p.type);
    }    
    document.getElementById("removePermission").disabled = !this._permissions.length;
    document.getElementById("removeAllPermissions").disabled = !this._permissions.length;

    if (!this._view.rowCount)
      this.clear();
  },

  onAllPermissionsDeleted: function ()
  {
    if (!this._view.rowCount)
      return;
    var removedPermissions = [];
    gTreeUtils.deleteAll(this._tree, this._view, this._permissions, removedPermissions);
    for (var i = 0; i < removedPermissions.length; ++i) {
      var p = removedPermissions[i];
      this._pm.remove(p.host, p.type);
    }
    document.getElementById("removePermission").disabled = true;
    document.getElementById("removeAllPermissions").disabled = true;
    this.clear();
  },

  onPermissionKeyPress: function (aEvent)
  {
    if (aEvent.keyCode == 46)
      this.onPermissionDeleted();
  },

  _lastPermissionSortColumn: "",
  _lastPermissionSortAscending: false,

  onPermissionSort: function (aColumn)
  {
    this._lastPermissionSortAscending = gTreeUtils.sort(this._tree, 
                                                        this._view, 
                                                        this._permissions,
                                                        aColumn, 
                                                        this._lastPermissionSortColumn, 
                                                        this._lastPermissionSortAscending);
    this._lastPermissionSortColumn = aColumn;
  },

  _loadPermissions: function ()
  {
    this._tree = document.getElementById("permissionsTree");
    this._permissions = [];

    // load permissions into a table
    var count = 0;
    var enumerator = this._pm.enumerator;
    while (enumerator.hasMoreElements()) {
      var nextPermission = enumerator.getNext().QueryInterface(Components.interfaces.nsIPermission);
      this._addPermissionToList(nextPermission);
    }

    this._view._rowCount = this._permissions.length;

    // sort and display the table
    this._tree.treeBoxObject.view = this._view;
    this.onPermissionSort("rawHost", false);

    // disable "remove all" button if there are none
    document.getElementById("removeAllPermissions").disabled = this._permissions.length == 0;
  },

  _addPermissionToList: function (aPermission)
  {
    if (aPermission.type == this._type) {
      var host = aPermission.host;
      var capabilityString = this._getCapabilityString(aPermission.capability);
      var p = new Permission(host,
                             (host.charAt(0) == ".") ? host.substring(1,host.length) : host,
                             aPermission.type,
                             capabilityString, 
                             aPermission.capability);
      this._permissions.push(p);
    }
  },

  setHost: function (aHost)
  {
    document.getElementById("url").value = aHost;
  },

  clear: function ()
  {
    document.getElementById("url").value = "";
    document.getElementById("checkLoad").checked = false;
    document.getElementById("checkSend").checked = false;
    document.getElementById("btnAdd").disabled = true;

    document.getElementById("btnAdd").hidden = false;
    document.getElementById("btnSave").hidden = true;
    document.getElementById("btnSave").disabled = false;

    this._tree.view.selection.clearSelection();
  }
};

function setHost(aHost)
{
  gPermissionManager.setHost(aHost);
}

function initWithParams(aParams)
{
  gPermissionManager.init(aParams);
}

