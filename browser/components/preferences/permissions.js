/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIPermissionManager = Components.interfaces.nsIPermissionManager;
const nsICookiePermission = Components.interfaces.nsICookiePermission;

const NOTIFICATION_FLUSH_PERMISSIONS = "flush-pending-permissions";

function Permission(host, rawHost, type, capability) 
{
  this.host = host;
  this.rawHost = rawHost;
  this.type = type;
  this.capability = capability;
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
        // Avoid calling the permission manager if the capability settings are
        // the same. Otherwise allow the call to the permissions manager to
        // update the listbox for us.
        exists = this._permissions[i].capability == capabilityString;
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

    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.notifyObservers(null, NOTIFICATION_FLUSH_PERMISSIONS, this._type);
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

      // Ignore unrelated permission types.
      if (permission.type != this._type)
        return;

      if (aData == "added") {
        this._addPermissionToList(permission);
        ++this._view._rowCount;
        this._tree.treeBoxObject.rowCountChanged(this._view.rowCount - 1, 1);        
        // Re-do the sort, since we inserted this new item at the end. 
        gTreeUtils.sort(this._tree, this._view, this._permissions,
                        this._permissionsComparator,
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
                          this._permissionsComparator,
                          this._lastPermissionSortColumn, 
                          this._lastPermissionSortAscending);
        }
        this._tree.treeBoxObject.invalidate();
      }
      else if (aData == "deleted") {
        for (var i = 0; i < this._permissions.length; i++) {
          if (this._permissions[i].host == permission.host) {
            this._permissions.splice(i, 1);
            this._view._rowCount--;
            this._tree.treeBoxObject.rowCountChanged(this._view.rowCount - 1, -1);
            this._tree.treeBoxObject.invalidate();
            break;
          }
        }
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
    if (aPermission.type == this._type &&
        (!this._manageCapability ||
         (aPermission.capability == this._manageCapability))) {

      var host = aPermission.host;
      var capabilityString = this._getCapabilityString(aPermission.capability);
      var p = new Permission(host,
                             (host.charAt(0) == ".") ? host.substring(1,host.length) : host,
                             aPermission.type,
                             capabilityString);
      this._permissions.push(p);
    }  
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

