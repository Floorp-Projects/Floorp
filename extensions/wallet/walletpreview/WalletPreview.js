/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* for localization */
var JS_STRINGS_FILE = "chrome://communicator/locale/wallet/WalletPreview.properties";
var bundle = srGetStrBundle(JS_STRINGS_FILE);
var heading = bundle.GetStringFromName("heading");
var bypass = bundle.GetStringFromName("bypass");
var doNotPrefill = bundle.GetStringFromName("doNotPrefill");

/* for xpconnect */
var walletpreview =
    Components.classes
      ["@mozilla.org/walletpreview/walletpreview-world;1"].createInstance();
walletpreview = walletpreview.QueryInterface(Components.interfaces.nsIWalletPreview);

var prefillList = [];
var fieldCount = 0;

function Startup() {

  /* fetch the input */

  list = walletpreview.GetPrefillValue();
  BREAK = list[0];
  prefillList = list.split(BREAK);

  /* create the heading */

  var heading = document.getElementById("walletpreview");
  heading.setAttribute("title", bundle.GetStringFromName("title"));
  var heading = document.getElementById("heading");
  heading.setAttribute("value", bundle.GetStringFromName("heading"));
  var fieldHeading = document.getElementById("fieldHeading");
  fieldHeading.setAttribute("value", bundle.GetStringFromName("fieldHeading"));
  var valueHeading = document.getElementById("valueHeading");
  valueHeading.setAttribute("value", bundle.GetStringFromName("valueHeading"));

  var menuPopup;
  var count;

  /* create the fill-in entries */

  for (i=1; i<prefillList.length-2; i+=3) {

    if(prefillList[i] != 0) {
      count = prefillList[i];
      menuPopup = document.createElement("menupopup");
//      menuList.setAttribute("size", Number(count)+1);
    }
    count--;
    var menuItem = document.createElement("menuitem");
    if (count == (prefillList[i]-1)) {
      menuItem.setAttribute("selected", "true");
    }
    menuItem.setAttribute("data", prefillList[i+1]);
    menuItem.setAttribute("value", prefillList[i+2]);
    menuPopup.appendChild(menuItem);

    if(count == 0) {
      var lastMenuItem = document.createElement("menuitem");
      lastMenuItem.setAttribute("data", prefillList[i+1]);
      lastMenuItem.setAttribute("value", "<"+doNotPrefill+">");
      menuPopup.appendChild(lastMenuItem);

      var menuList = document.createElement("menulist");
      menuList.setAttribute("id", "x"+(++fieldCount));
      menuList.setAttribute("allowevents", "true");
      menuList.appendChild(menuPopup);

      var treeCell0 = document.createElement("treecell");
      treeCell0.setAttribute("value", prefillList[i+1])

      var treeCell = document.createElement("treecell");
      treeCell.appendChild(menuList);

      var treeRow = document.createElement("treerow");
      treeRow.appendChild(treeCell0);
      treeRow.appendChild(treeCell);

      var treeItem = document.createElement("treeitem");
      treeItem.appendChild(treeRow);

      var treeChildren = document.getElementById("combolists");
      treeChildren.appendChild(treeItem);
    }
  }

  /* create checkbox label */

  var checkBox = document.getElementById("checkbox");
  checkBox.setAttribute("value", bypass);

  /* initialization OK and Cancel buttons */

  doSetOKCancel(Save, Cancel);
}

function Save() {
  var list = prefillList[prefillList.length-2];
  var url = prefillList[prefillList.length-1];
  var fillins = "";

  for (i=1; i<=fieldCount; i++) { 
    var menuList = document.getElementById("x" + i);
    fillins +=
      menuList.selectedItem.getAttribute("data") + "#*%$" +
      menuList.selectedItem.getAttribute("value") + "#*%$";
  }

  var checkBox = document.getElementById("checkbox");
  var checked = checkBox.checked;

  var result = "|list|"+list+"|fillins|"+fillins+"|url|"+url+"|skip|"+checked+"|";
  walletpreview.SetValue(result, window);
  return true;
}

function Cancel() {
  var list = prefillList[prefillList.length-2];
  var result = "|list|"+list+"|fillins||url||skip|false|";
  walletpreview.SetValue(result, window);
  return true;
}
