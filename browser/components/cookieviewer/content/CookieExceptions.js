const nsIPermissionManager = Components.interfaces.nsIPermissionManager;
const nsICookiePermission = Components.interfaces.nsICookiePermission;

function Permission(id, host, rawHost, type, capability, perm) 
{
  this.id = id;
  this.host = host;
  this.rawHost = rawHost;
  this.type = type;
  this.capability = capability;
  this.perm = perm;
}

var gPermissionManager = {
  _type                 : "",
  _addedPermissions     : [],
  _removedPermissions   : [],
  _pm                   : Components.classes["@mozilla.org/permissionmanager;1"]
                                    .getService(Components.interfaces.nsIPermissionManager),
  _bundle               : null,
  _tree                 : null,
  
  _view: {
    _rowCount: 0,
    get rowCount() 
    { 
      return this._rowCount; 
    },
    getCellText: function (aRow, aColumn)
    {
      if (aColumn == "siteCol")
        return gPermissionManager._addedPermissions[aRow].rawHost;
      else if (aColumn == "statusCol")
        return gPermissionManager._addedPermissions[aRow].capability;
      return "";
    },

    isSeparator: function(aIndex) { return false; },
    isSorted: function() { return false; },
    isContainer: function(aIndex) { return false; },
    setTree: function(aTree){},
    getImageSrc: function(aRow, aColumn) {},
    getProgressMode: function(aRow, aColumn) {},
    getCellValue: function(aRow, aColumn) {},
    cycleHeader: function(aColId, aElt) {},
    getRowProperties: function(aRow, aColumn, aProperty) {},
    getColumnProperties: function(aColumn, aColumnElement, aProperty) {},
    getCellProperties: function(aRow, aProperty) {}
  },
  
  onOK: function ()
  {
    var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                       .getService(Components.interfaces.nsIPermissionManager);
    for (var i = 0; i < this._removedPermissions.length; ++i) {
      var p = this._removedPermissions[i];
      pm.remove(p.host, p.type);
    }

    var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIURI);    
    for (var i = 0; i < this._addedPermissions.length; ++i) {
      var p = this._addedPermissions[i];
      uri.spec = p.host;
      pm.add(uri, p.type, p.perm);
    }
  },
  
  addPermission: function (aPermission)
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
      var message = stringBundle.getString("invalidURI");
      var title = stringBundle.getString("invalidURITitle");
      promptservice.alert(window,title,message);
    }

    // we need this whether the perm exists or not
    var stringKey = null;
    switch (aPermission) {
      case nsIPermissionManager.ALLOW_ACTION:
        stringKey = "can";
        break;
      case nsIPermissionManager.DENY_ACTION:
        stringKey = "cannot";
        break;
      case nsICookiePermission.ACCESS_SESSION:
        stringKey = "canSession";
        break;
      default:
        break;
    } 
    // check whether the permission already exists, if not, add it
    var exists = false;
    for (var i = 0; i < this._addedPermissions.length; ++i) {
      if (this._addedPermissions[i].rawHost == host) {
        exists = true;
        this._addedPermissions[i].capability = this._bundle.getString(stringKey);
        this._addedPermissions[i].perm = aPermission;
        break;
      }
    }
    
    if (!exists) {
      var p = new Permission(this._addedPermissions.length, 
                             host, 
                             (host.charAt(0) == ".") ? host.substring(1,host.length) : host, 
                             this._type, 
                             this._bundle.getString(stringKey), 
                             aPermission);
      this._addedPermissions.push(p);
      
      this._view._rowCount = this._addedPermissions.length;
      this._tree.treeBoxObject.rowCountChanged(this._addedPermissions.length-1, 1);
      this._tree.treeBoxObject.ensureRowIsVisible(this._addedPermissions.length-1);
    }
    textbox.value = "";
    textbox.focus();

    // covers a case where the site exists already, so the buttons don't disable
    this.onHostInput(textbox);

    // enable "remove all" button as needed
    document.getElementById("removeAllPermissions").disabled = this._addedPermissions.length == 0;
  },
  
  onHostInput: function (aSiteField)
  {
    document.getElementById("btnSession").disabled = !aSiteField.value;
    document.getElementById("btnBlock").disabled = !aSiteField.value;
    document.getElementById("btnAllow").disabled = !aSiteField.value;
  },
  
  onLoad: function ()
  {
    this._type = window.arguments[0].permissionType;
    this._bundle = document.getElementById("permBundle");

    var permissionsText = document.getElementById("permissionsText");
    while (permissionsText.hasChildNodes())
      permissionsText.removeChild(permissionsText.firstChild);
    
    var introString = this._bundle.getString(this._type + "permissionstext");
    permissionsText.appendChild(document.createTextNode(introString));

    var titleString = this._bundle.getString(this._type + "permissionstitle");
    document.documentElement.setAttribute("title", titleString);
    
    document.getElementById("btnBlock").hidden = !window.arguments[0].blockVisible;
    document.getElementById("btnSession").hidden = !window.arguments[0].sessionVisible;
    document.getElementById("btnAllow").hidden = !window.arguments[0].allowVisible;
    document.getElementById("url").value = window.arguments[0].prefilledHost;
    this.onHostInput(document.getElementById("url"));

    this._loadPermissions();
  },
  
  onPermissionSelected: function ()
  {
    var selections = GetTreeSelections(this._tree);
    document.getElementById("removePermission").disabled = (selections.length < 1);
  },
  
  onPermissionDeleted: function ()
  {
    DeleteSelectedItemFromTree(this._tree, this._view,
                               this._addedPermissions, this._removedPermissions,
                               "removePermission", "removeAllPermissions");
  },
  
  onAllPermissionsDeleted: function ()
  {
    DeleteAllFromTree(this._tree, this._view,
                      this._addedPermissions, this._removedPermissions,
                      "removePermission", "removeAllPermissions");
  },
  
  onPermissionKeyPress: function (aEvent)
  {
    if (aEvent.keyCode == 46)
      this.onPermissionDeleted();
  },
  
  _lastPermissionSortColumn: "",
  _lastPermissionSortAscending: false,
  
  onPermissionSort: function (aColumn, aUpdateSelection)
  {
    this._lastPermissionSortAscending = SortTree(this._tree, 
                                                 this._view, 
                                                 this._addedPermissions,
                                                 aColumn, 
                                                 this._lastPermissionSortColumn, 
                                                 this._lastPermissionSortAscending, 
                                                 aUpdateSelection);
    this._lastPermissionSortColumn = aColumn;
  },
  
  _loadPermissions: function ()
  {
    this._tree = document.getElementById("permissionsTree");

    // load permissions into a table
    var count = 0;
    var enumerator = this._pm.enumerator;
    while (enumerator.hasMoreElements()) {
      var nextPermission = enumerator.getNext().QueryInterface(Components.interfaces.nsIPermission);
      if (nextPermission.type == this._type) {
        var host = nextPermission.host;
        var capability = null;
        switch (nextPermission.capability) {
          case nsIPermissionManager.ALLOW_ACTION:
            capability = "can";
            break;
          case nsIPermissionManager.DENY_ACTION:
            capability = "cannot";
            break;
          // we should only ever hit this for cookies
          case nsICookiePermission.ACCESS_SESSION:
            capability = "canSession";
            break;
          default:
            break;
        } 
        var capabilityString = this._bundle.getString(capability);
        var p = new Permission(count++, host,
                               (host.charAt(0) == ".") ? host.substring(1,host.length) : host,
                               nextPermission.type,
                               capabilityString, 
                               nextPermission.capability);
        this._addedPermissions.push(p);
      }
    }
   
    this._view._rowCount = this._addedPermissions.length;

    // sort and display the table
    this._tree.treeBoxObject.view = this._view;
    this.onPermissionSort("rawHost", false);

    // disable "remove all" button if there are none
    document.getElementById("removeAllPermissions").disabled = this._addedPermissions.length == 0;
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

