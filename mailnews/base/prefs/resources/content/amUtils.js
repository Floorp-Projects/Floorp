/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Varada Parthasarathi <varada@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  if (ret == nsIFilePicker.returnOK) 
  {
    // check that no other account/server has this same local directory
    var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
    var allServers = accountManager.allServers;

    for (var i=0; i < allServers.Count(); i++)
    {
      var currentServer = allServers.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
      var localPath = currentServer.localPath;
      if (currentServer.key != gServer.key && fp.file.path == localPath.nativePath)
      {
        var directoryAlreadyUsed =
          top.gPrefsBundle.getString("directoryUsedByOtherServer");

        var promptService =
          Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
                   getService(Components.interfaces.nsIPromptService);
        promptService.alert(window, null, directoryAlreadyUsed);
        return;
      }
    }
  }
  // convert the nsILocalFile into a nsIFileSpec 
  currentFolderTextBox.value = fp.file.path;
}

function hostnameIsIllegal(hostname)
{
  // XXX TODO do a complete check.
  // this only checks for illegal characters in the hostname
  // but hostnames like "...." and "_" and ".111" will get by
  // my test.  
  var validChars = hostname.match(/[A-Za-z0-9.-]/g);
  if (!validChars || (validChars.length != hostname.length)) {
    return true;
  }

  return false;
}

