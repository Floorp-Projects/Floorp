/* The contents of this file are subject to the Netscape Public
  License Version 1.1 (the "License"); you may not use this file
  except in compliance with the License. You may obtain a copy of
  the License at http://www.mozilla.org/NPL/
  
  Software distributed under the License is distributed on an "AS
  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
  implied. See the License for the specific language governing
  rights and limitations under the License.
  
  The Original Code is Mozilla Communicator client code, released
  March 31, 1998.
  
  The Initial Developer of the Original Code is Netscape
  Communications Corporation. Portions created by Netscape are
  Copyright (C) 1998-1999 Netscape Communications Corporation. All
  Rights Reserved.
  
  Contributor(s): 
    David Hyatt (hyatt@apple.com)
*/

var gSiteBox;
var gUnblockButton;
var gPageReport;

function onLoad()
{
  gSiteBox = document.getElementById("siteBox");
  gUnblockButton = document.getElementById("unblockButton");
  gPageReport = opener.gBrowser.pageReport;

  buildSiteBox();
}

function buildSiteBox()
{
  for (var i = 0; i < gPageReport.length; i++) {
    var found = false;
    for (var j = 0; j < gSiteBox.childNodes.length; j++) {
      if (gSiteBox.childNodes[i].label == gPageReport[i]) {
        found = true;
        break;
      }
    }

    if (found) continue;

    var listitem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                             "listitem");
    listitem.setAttribute("label", gPageReport[i]);
    gSiteBox.appendChild(listitem);
  }
}

function siteSelected()
{
  gUnblockButton.disabled = (gSiteBox.selectedItems.length == 0);
}

