# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Dan Mosedale <dmose@mozilla.org>
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

var gDownloadsPane = {
  chooseFolder: function ()
  {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(nsIFilePicker);
    var bundlePreferences = document.getElementById("bundlePreferences");
    var title = bundlePreferences.getString("chooseDownloadFolderTitle");
    fp.init(window, title, nsIFilePicker.modeGetFolder);
    
    const nsILocalFile = Components.interfaces.nsILocalFile;
    var customDirPref = document.getElementById("browser.download.dir");
    if (customDirPref.value)
      fp.displayDirectory = customDirPref.value;
    fp.appendFilters(nsIFilePicker.filterAll);
    if (fp.show() == nsIFilePicker.returnOK) {
      var file = fp.file.QueryInterface(nsILocalFile);
      var currentDirPref = document.getElementById("browser.download.downloadDir");
      customDirPref.value = currentDirPref.value = file;
      var folderListPref = document.getElementById("browser.download.folderList");
      folderListPref.value = this._fileToIndex(file);
    }
  },
  
  onReadUseDownloadDir: function ()
  {
    var downloadFolder = document.getElementById("downloadFolder");
    var chooseFolder = document.getElementById("chooseFolder");
    var preference = document.getElementById("browser.download.useDownloadDir");
    downloadFolder.disabled = !preference.value;
    chooseFolder.disabled = !preference.value;      
    return undefined;
  },
  
  _fileToIndex: function (aFile)
  { 
    if (!aFile || aFile.equals(this._getDownloadsFolder("Desktop")))
      return 0;
    else if (aFile.equals(this._getDownloadsFolder("Downloads")))
      return 1;
    return 2;
  },
  
  _indexToFile: function (aIndex)
  {
    switch (aIndex) {
    case 0: 
      return this._getDownloadsFolder("Desktop");
    case 1:
      return this._getDownloadsFolder("Downloads");
    }
    var customDirPref = document.getElementById("browser.download.dir");
    return customDirPref.value;
  },
  
  _getSpecialFolderKey: function (aFolderType)
  {
#ifdef XP_WIN
    return aFolderType == "Desktop" ? "DeskP" : "Pers";
#endif
#ifdef XP_MACOSX
    return aFolderType == "Desktop" ? "UsrDsk" : "UsrDocs";
#endif
#ifdef XP_OS2
    return aFolderType == "Desktop" ? "Desk" : "Home";
#endif
    return "Home";
  },

  _getDownloadsFolder: function (aFolder)
  {
    var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"]
                                .getService(Components.interfaces.nsIProperties);
    var dir = fileLocator.get(this._getSpecialFolderKey(aFolder), 
                              Components.interfaces.nsILocalFile);
    if (aFolder != "Desktop")
      dir.append("My Downloads");
      
    return dir;
  },
  
  _getDisplayNameOfFile: function (aFolder)
  {
    // TODO: would like to add support for 'Downloads on Macintosh HD' 
    //       for OS X users.
    return aFolder ? aFolder.path : "";
  },
  
  readDownloadDirPref: function ()
  {
    var folderListPref = document.getElementById("browser.download.folderList");
    var bundlePreferences = document.getElementById("bundlePreferences");
    var downloadFolder = document.getElementById("downloadFolder");

    var customDirPref = document.getElementById("browser.download.dir");
    var customIndex = customDirPref.value ? this._fileToIndex(customDirPref.value) : 0;
    if (folderListPref.value == 0 || customIndex == 0)
      downloadFolder.label = bundlePreferences.getString("desktopFolderName");
    else if (folderListPref.value == 1 || customIndex == 1) 
      downloadFolder.label = bundlePreferences.getString("myDownloadsFolderName");
    else
      downloadFolder.label = this._getDisplayNameOfFile(customDirPref.value);
    
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
    var fph = ios.getProtocolHandler("file")
                 .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
    var currentDirPref = document.getElementById("browser.download.downloadDir");
    var downloadDir = currentDirPref.value || this._indexToFile(folderListPref.value);
    var urlspec = fph.getURLSpecFromFile(downloadDir);
    downloadFolder.image = "moz-icon://" + urlspec + "?size=16";
    
    return undefined;
  },
  
  writeFolderList: function ()
  {
    var currentDirPref = document.getElementById("browser.download.downloadDir");
    return this._fileToIndex(currentDirPref.value);
  },
  
  showWhenStartingPrefChanged: function ()
  {
    document.getElementById("browser.download.manager.closeWhenDone")
            .disabled = !document.getElementById("browser.download.manager.showWhenStarting").value;
  },
  
  readShowWhenStartingPref: function ()
  {
    this.showWhenStartingPrefChanged();
    return undefined;
  },
  
  showFileTypeActions: function ()
  {
    document.documentElement.openWindow("Preferences:DownloadActions",
                                        "chrome://messenger/content/preferences/downloadactions.xul",
                                        "", null);
  }
};
