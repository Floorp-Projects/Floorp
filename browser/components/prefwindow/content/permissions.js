# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#   Blake Ross (original author)
#

var permissionsTree;

var permissions           = [];
var deletedPermissions   = [];
var permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
permissionmanager = permissionmanager.QueryInterface(Components.interfaces.nsIPermissionManager);
var permBundle;

const nsIPermissionManager = Components.interfaces.nsIPermissionManager;

function GetFields()
{
  var dataObject = {};
  dataObject.permissions = permissions;
  dataObject.deletedPermissions = deletedPermissions;
  return dataObject;
}

function SetFields(data)
{
  var win;
  if ('opener' in window && window.opener)
    win = window.opener.top;
  else
    win = window.top;

  if ('permissions' in data)
    permissions = data.permissions;  

  if ('deletedPermissions' in data)
    deletedPermissions = data.deletedPermissions;
}
 
var permissionsTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    if (column=="siteCol") {
      rv = permissions[row].rawHost;
    } else if (column=="statusCol") {
      rv = permissions[row].capability;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(aColId, aElt) {},
  getRowProperties : function(row,column,prop){},
  getColumnProperties : function(column,columnElement,prop){},
  getCellProperties : function(row,prop){}
 };

function Permission(number, host, rawHost, type, capability, perm) {
  this.number = number;
  this.host = host;
  this.rawHost = rawHost;
  this.type = type;
  this.capability = capability;
  this.perm = perm;
}

function PermissionSelected() {
  var selections = GetTreeSelections(permissionsTree);
  document.getElementById("removePermission").disabled = (selections.length < 1);
}

function DeletePermission() {
  DeleteSelectedItemFromTree(permissionsTree, permissionsTreeView,
                             permissions, deletedPermissions,
                             "removePermission", "removeAllPermissions");
}

function DeleteAllPermissions() {
  DeleteAllFromTree(permissionsTree, permissionsTreeView,
                    permissions, deletedPermissions,
                    "removePermission", "removeAllPermissions");
}

function HandlePermissionKeyPress(e) {
  if (e.keyCode == 46) {
    DeletePermission();
  }
}

var lastPermissionSortColumn = "";
var lastPermissionSortAscending = false;

function PermissionColumnSort(column, updateSelection) {
  lastPermissionSortAscending =
    SortTree(permissionsTree, permissionsTreeView, permissions,
                 column, lastPermissionSortColumn, lastPermissionSortAscending, 
                 updateSelection);
  lastPermissionSortColumn = column;
}

function loadPermissions() {
  if (!permBundle)
    permBundle = document.getElementById("permBundle");

  if (!permissionsTree)
    permissionsTree = document.getElementById("permissionsTree");

  var win;
  if ('opener' in window && window.opener)
    win = window.opener.top;
  else
    win = window.top;

  var dataObject = win.hPrefWindow.wsm.dataManager.pageData[window.location.href].userData;

  // load permissions into a table
  if (!('permissions' in dataObject)) {
    var enumerator = permissionmanager.enumerator;
    var count = 0;
    while (enumerator.hasMoreElements()) {
      var nextPermission = enumerator.getNext();
      nextPermission = nextPermission.QueryInterface(Components.interfaces.nsIPermission);
      if (nextPermission.type == permType) {
        var host = nextPermission.host;
        var capability = ( nextPermission.capability == nsIPermissionManager.ALLOW_ACTION ) ? true : false;
        permissions[count] = new Permission(count++, host,
                         (host.charAt(0)==".") ? host.substring(1,host.length) : host,
                         nextPermission.type,
                         permBundle ? permBundle.getString(capability?"can":"cannot") : "", nextPermission.capability);
      }
    }
  }

  permissionsTreeView.rowCount = permissions.length;

  // sort and display the table
  permissionsTree.treeBoxObject.view = permissionsTreeView;
  PermissionColumnSort('rawHost', false);

  // disable "remove all" button if there are none
  document.getElementById("removeAllPermissions").disabled = permissions.length == 0;
}



