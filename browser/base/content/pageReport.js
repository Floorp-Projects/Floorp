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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   David Hyatt  (hyatt@apple.com)
#   Dean Tessman (dean_tessman@hotmail.com)
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

var gSiteBox;
var gUnblockButton;
var gPageReport;

var permissionmanager =
        Components.classes["@mozilla.org/permissionmanager;1"]
          .getService(Components.interfaces.nsIPermissionManager);
var nsIPermissionManager = Components.interfaces.nsIPermissionManager;

function onLoad()
{
  gSiteBox = document.getElementById("siteBox");
  gUnblockButton = document.getElementById("unblockButton");
  gPageReport = opener.gBrowser.pageReport;

  buildSiteBox();
  // select the first item using a delay, otherwise the listitems
  // don't paint as selected.
  setTimeout(selectFirstItem, 0);
}

function selectFirstItem()
{
  gSiteBox.selectedIndex = 0;
}

function buildSiteBox()
{
  for (var i = 0; i < gPageReport.length; i++) {
    var found = false;
    for (var j = 0; j < gSiteBox.childNodes.length; j++) {
      if (gSiteBox.childNodes[j].label == gPageReport[i]) {
        found = true;
        break;
      }
    }

    if (!found)
      gSiteBox.appendItem(gPageReport[i]);
  }
}

function siteSelected()
{
  gUnblockButton.disabled = (gSiteBox.selectedItems.length == 0);
}

function whitelistSite()
{
  var selectedItem = gSiteBox.selectedItems[0];
  if (!selectedItem)
    return;

  var selectedIndex = gSiteBox.getIndexOfItem(selectedItem);

  var uri = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService)
                      .newURI(selectedItem.label, null, null);

  permissionmanager.add(uri, "popup", nsIPermissionManager.ALLOW_ACTION);
  gSiteBox.removeChild(selectedItem);

  if (gSiteBox.getRowCount() == 0) {
    // close if there are no other sites to whitelist
    window.close();
    return;
  }

  // make sure a site is selected
  if (selectedIndex > gSiteBox.getRowCount() - 1)
    selectedIndex -= 1;
  gSiteBox.selectedIndex = selectedIndex;
  document.documentElement.getButton("accept").focus()
}
