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
 * Jason Kersey (kerz@netscape.com)
 */

/* for xpconnect */
var walletpreview =
    Components.classes
      ["@mozilla.org/walletpreview/walletpreview-world;1"].createInstance();
walletpreview = walletpreview.QueryInterface(Components.interfaces.nsIWalletPreview);

var prefillList = [];
var fieldCount = 0;

function Startup() {

  /* fetch the input */

  var list = walletpreview.GetPrefillValue();
  var BREAK = list[0];
  prefillList = list.split(BREAK);

  var menuPopup;
  var count;

  /* create the fill-in entries */

  for (var i=1; i<prefillList.length-1; i+=3) {

    if(prefillList[i] != 0) {
      count = prefillList[i];
      menuPopup = document.createElement("menupopup");
    }
    count--;
    var menuItem = document.createElement("menuitem");
    if (count == (prefillList[i]-1)) {
      menuItem.setAttribute("selected", "true");
    }
    menuItem.setAttribute("label", prefillList[i+2]);

    // Avoid making duplicate entries in the same menulist
    var child = menuPopup.firstChild;
    var alreadyThere = false;
    while (child) {
      if (child.getAttribute("label") == prefillList[i+2]) {
        alreadyThere = true;
        break;
      }
      child = child.nextSibling;
    }
    if (!alreadyThere) {
      menuPopup.appendChild(menuItem);
    }

    if(count == 0) {
      //create the menulist of form data options.
      var menuList = document.createElement("menulist");
      menuList.setAttribute("data", prefillList[i+1]);
      menuList.setAttribute("id", "xx"+(++fieldCount));
      menuList.setAttribute("allowevents", "true");
      menuList.appendChild(menuPopup);

      //create the checkbox for the menulist.
      var localCheckBox = document.createElement("checkbox");
      localCheckBox.setAttribute("id", "x"+fieldCount);
      // Note: menulist name is deliberately chosen to be x + checkbox name in
      //       order to make it easy to get to menulist from associated checkbox
      localCheckBox.setAttribute("oncommand", "UpdateMenuListEnable(this)");
      localCheckBox.setAttribute("checked", "true");
      localCheckBox.setAttribute("crop", "right");
      
      //fix label so it only shows title of field, not any of the url.
      var colonPos = prefillList[i+1].indexOf(':');
      var checkBoxLabel;
      if (colonPos != -1)
        checkBoxLabel = prefillList[i+1].slice(colonPos + 1)
      else
        checkBoxLabel = prefillList[i+1];
      localCheckBox.setAttribute("label", checkBoxLabel );

      //append all the items into the row.
      var row = document.createElement("row");
      row.appendChild(localCheckBox);
      row.appendChild(menuList);

      var rows = document.getElementById("rows");
      rows.appendChild(row);

      // xul bug: if this is done earlier, it will result in a crash (???)
      menuList.setAttribute("editable", "true");

    }
  }

  /* initialization OK and Cancel buttons */

  doSetOKCancel(Save, Cancel);
}

function UpdateMenuListEnable(checkBox) {
  var id = checkBox.getAttribute("id");
  var menuList = document.getElementById("x" + id);
  var menuItem = menuList.selectedItem;
  if (checkBox.checked) {
    menuList.removeAttribute("disabled");
    menuList.setAttribute("editable", "true");
  } else {
    menuList.setAttribute("disabled", "true");
    menuList.removeAttribute("editable");
  }
  menuList.selectedItem = menuItem;
}

function EncodeVerticalBars(s) {
  s = s.replace(/\^/g, "^1");
  s = s.replace(/\|/g, "^2");
  return s;
}

function Save() {
  var url = prefillList[prefillList.length-1];
  var fillins = "";

  for (var i=1; i<=fieldCount; i++) { 
    var menuList = document.getElementById("xx" + i);
    if (menuList)
    {
      fillins += menuList.getAttribute("data")  + "#*%$";
      var localCheckBox = document.getElementById("x" + i);
      if (localCheckBox.checked) {
        fillins += menuList.value;
      }
      fillins += "#*%$";
    }
  }

  var checkBox = document.getElementById("checkbox");
  var checked = checkBox.checked;

  var result = "|fillins|"+EncodeVerticalBars(fillins)+
               "|url|"+EncodeVerticalBars(url)+"|skip|"+checked+"|";
  walletpreview.SetValue(result, window);
  return true;
}

function Cancel() {
  var list = prefillList[prefillList.length-2];
  var result = "|list|"+list+"|fillins||url||skip|false|";
  walletpreview.SetValue(result, window);
  return true;
}
