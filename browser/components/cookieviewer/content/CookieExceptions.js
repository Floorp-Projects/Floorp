# -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released March
# 31, 1998.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#   Blake Ross (original author)

var permType = "cookie";

function onOK() {
  window.opener.top.wsm.savePageData(window.location.href, window);
  window.opener.top.hPrefWindow.registerOKCallbackFunc(window.opener.onPrefsOK);
}

function manageSite(perm)
{
  var textbox = document.getElementById("url");
  var url = textbox.value.replace(/ /g, "");
  var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                      .createInstance(Components.interfaces.nsIURI);
  uri.spec = url;

  var exists = false;
  for (var i = 0; i < permissions.length; ++i) {
    if (permissions[i].rawHost == url) {
      exists = true;
      permissions[i].capability = permBundle.getString(perm == permissionmanager.ALLOW_ACTION?"can":"cannot");
      permissions[i].perm = perm;
      break;
    }
  }
  
  if (!exists) {
    permissions[permissions.length] = new Permission(permissions.length, url, (url.charAt(0)==".") ? url.substring(1,url.length) : url, "cookie", permBundle.getString(perm==permissionmanager.ALLOW_ACTION?"can":"cannot"), perm);
    permissionsTreeView.rowCount = permissions.length;
    permissionsTree.treeBoxObject.rowCountChanged(permissions.length-1, 1);
    permissionsTree.treeBoxObject.ensureRowIsVisible(permissions.length-1)

  }
  textbox.value = "";
  textbox.focus();
}

function buttonEnabling(textfield)
{
  var block = document.getElementById("btnBlock");
  var allow = document.getElementById("btnAllow");
  block.disabled = !textfield.value;
  allow.disabled = !textfield.value;
}

function Startup()
{
  loadPermissions();
}


