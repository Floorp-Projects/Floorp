/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

function onInit() {
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsILocalFile = Components.interfaces.nsILocalFile;
const LOCALFILE_CTRID = "@mozilla.org/file/local;1";

function BrowseForLocalFolders()
{
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  var currentFolder = Components.classes[LOCALFILE_CTRID].createInstance(nsILocalFile);
  var currentFolderTextBox = document.getElementById("server.localPath");

  currentFolder.initWithPath(currentFolderTextBox.value);

  fp.init(window, document.getElementById("browseForLocalFolder").getAttribute("filepickertitle"), nsIFilePicker.modeGetFolder);

  fp.displayDirectory = currentFolder;

  var ret = fp.show();

  if (ret == nsIFilePicker.returnOK) {
    // convert the nsILocalFile into a nsIFileSpec 
    currentFolderTextBox.value = fp.file.path;
  }
}
