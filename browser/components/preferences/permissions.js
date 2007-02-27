# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
const nsICookiePermission = Components.interfaces.nsICookiePermission;

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
  _tree         : null,
  
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
    case nsIPermissionManager.ALLOW_ACTION:
      stringKey = "can";
      break;
    case nsIPermissionManager.DENY_ACTION:
      stringKey = "cannot";
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

    var capabilityString = this._getCapabilityString(aCapability);

    // check whether the permission already exists, if not, add it
    var exists = false;
    for (var i = 0; i < this._permissions.length; ++i) {
      if (this._permissions[i].rawHost == host) {
        exists = true;
        this._permissions[i].capability = capabilityString;
        this._permissions[i].perm = aCapability;
        break;
      }
    }
    
    if (!exists) {
      host = (host.charAt(0) == ".") ? host.substring(1,host.length) : host;
      var uri = ioService.newURI("http://" + host, null, null);
      this._pm.add(uri, this._type, aCapability);
    }
    textbox.value = "";
    textbox.focus();

    // covers a case where the site exists already, so the buttons don't disable
    this.onHostInput(textbox);

    // enable "remove all" button as needed
    document.getElementById("removeAllPermissions").disabled = this._permissions.length == 0;
  },
  
  onHostInput: function (aSiteField)
  {
    document.getElementById("btnSession").disabled = !aSiteField.value;
    document.getElementById("btnBlock").disabled = !aSiteField.value;
    document.getElementById("btnAllow").disabled = !aSiteField.value;
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
    
    var permissionsText = document.getElementById("permissionsText");
    while (permissionsText.hasChildNodes())
      permissionsText.removeChild(permissionsText.firstChild);
    permissionsText.appendChild(document.createTextNode(aParams.introText));

    document.title = aParams.windowTitle;
    
    document.getElementById("btnBlock").hidden    = !aParams.blockVisible;
    document.getElementById("btnSession").hidden  = !aParams.sessionVisible;
    document.getElementById("btnAllow").hidden    = !aParams.allowVisible;
    
    var urlField = document.getElementById("url");
    urlField.value = aParams.prefilledHost;
    
    this.onHostInput(urlField);
    
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "perm-changed", false);

    if (this._type == "install") {
      var enumerator = this._pm.enumerator;
      if (!enumerator.hasMoreElements())
        this._updatePermissions();
    }

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
  
  _updatePermissions: function ()
  {
    try {
      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(Components.interfaces.nsIIOService);
      var pbi = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch2);
      var prefList = [["xpinstall.whitelist.add", nsIPermissionManager.ALLOW_ACTION],
                      ["xpinstall.whitelist.add.103", nsIPermissionManager.ALLOW_ACTION],
                      ["xpinstall.blacklist.add", nsIPermissionManager.DENY_ACTION]];

      for (var i = 0; i < prefList.length; ++i) {
        try {
          // this pref is a comma-delimited list of hosts
          var hosts = pbi.getCharPref(prefList[i][0]);
        } catch(ex) {
          continue;
        }

        if (!hosts)
          continue;

        hostList = hosts.split(",");
        var capability = prefList[i][1];
        for (var j = 0; j < hostList.length; ++j) {
          // trim leading and trailing spaces
          var host = hostList[j].replace(/^\s*/,"").replace(/\s*$/,"");
          try {
            var uri = ioService.newURI("http://" + host, null, null);
            this._pm.add(uri, this._type, capability);
          } catch(ex) { }
        }
        pbi.setCharPref(prefList[i][0], "");
      }
    } catch(ex) { }
  },
  
  setHost: function (aHost)
  {
    document.getElementById("url").value = aHost;
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

