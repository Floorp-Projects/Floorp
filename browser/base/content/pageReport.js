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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
#   David Hyatt  (hyatt@apple.com)
#   Dean Tessman (dean_tessman@hotmail.com)

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

  var uri = Components.classes['@mozilla.org/network/standard-url;1'].createInstance(Components.interfaces.nsIURI);
  uri.spec = selectedItem.label;
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
