/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 *  The contents of this file are subject to the Netscape Public
 *  License Version 1.1 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 *  the License at http://www.mozilla.org/NPL/
 *
 *  Software distributed under the License is distributed on an "AS
 *  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *  implied. See the License for the specific language governing
 *  rights and limitations under the License.
 *
 *  The Original Code is Mozilla Communicator client code, released
 *  March 31, 1998.
 *
 *  The Initial Developer of the Original Code is Netscape
 *  Communications Corporation. Portions created by Netscape are
 *  Copyright (C) 1998-1999 Netscape Communications Corporation. All
 *  Rights Reserved.
 *
 */

var dialog;

function onLoad()
{
  var arguments = window.arguments[0];

  dialog = {
    OKButton: document.getElementById("ok"),
    okCallback: arguments.okCallback
  };

  // set override checkbox
  document.getElementById("folderCharsetOverride").checked = arguments.folderCharsetOverride;
//  dump("folderCharsetOverride " + arguments.folderCharsetOverride + "\n");

  // get charset title (i.e. localized name), needed in order to set value for the menu
  var ccm = Components.classes['@mozilla.org/charset-converter-manager;1'];
  ccm = ccm.getService();
  ccm = ccm.QueryInterface(Components.interfaces.nsICharsetConverterManager2);
  // get a localized string
  var charsetTitle = ccm.GetCharsetTitle(ccm.GetCharsetAtom(arguments.folderCharset));

  // select the menu item 
  var folderCharsetList = document.getElementById("folderCharsetList");
  folderCharsetList.setAttribute("value", charsetTitle);
  folderCharsetList.setAttribute("data", arguments.folderCharset);
//  dump("folderCharset " + arguments.folderCharset + "\n");

  // pre select the folderPicker, based on what they selected in the folder pane
  dialog.preselectedFolderURI = arguments.preselectedURI;

  doSetOKCancel(onOK, onCancel);
}

function onOK()
{
  var folderCharsetList = document.getElementById("folderCharsetList");
  var characterSet = folderCharsetList.getAttribute("data");

  dialog.okCallback(document.getElementById("folderCharsetOverride").checked, 
                    characterSet, dialog.preselectedFolderURI);

  return true;
}

function onCancel()
{
  // close the window
  return true;
}
