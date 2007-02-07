/*
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#     Scott MacGregor <mscott@mozilla.org>
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
*/
const KEY_APPDIR = "XCurProcD";
const KEY_PROFILEDIR = "PrefD";

function onInit()
{
  // manually adjust several pref UI elements
  document.getElementById('spamLevel').checked =
    document.getElementById('server.spamLevel').value > 0;
    
  var spamActionTargetAccount = document.getElementById('server.spamActionTargetAccount').value;
  if (!spamActionTargetAccount)
  {
    var server = GetMsgFolderFromUri(parent.pendingServerId, false).server;
    if (server.canCreateFoldersOnServer && server.canSearchMessages)
      spamActionTargetAccount = parent.pendingServerId;
    else
      spamActionTargetAccount = parent.accountManager.localFoldersServer.serverURI;
    document.getElementById('server.spamActionTargetAccount').value = spamActionTargetAccount;
  }
  SetFolderPicker(spamActionTargetAccount, 'actionTargetAccount');
  var spamActionTargetFolder = document.getElementById('server.spamActionTargetFolder').value;
  if (!spamActionTargetFolder)
  {
    spamActionTargetFolder = parent.accountManager.localFoldersServer.serverURI + "/Junk";
    document.getElementById('server.spamActionTargetFolder').value = spamActionTargetFolder;
  }
  SetFolderPicker(spamActionTargetFolder, 'actionTargetFolder');
  
  // set up the whitelist UI
  document.getElementById("whiteListAbURI").value =
    document.getElementById("server.whiteListAbURI").value;
  
  // set up trusted IP headers
  var serverFilterList = document.getElementById("useServerFilterList");
  serverFilterList.value =
    document.getElementById("server.serverFilterName").value;
  if (!serverFilterList.selectedItem)
    serverFilterList.selectedIndex = 0;
   
  updateMoveTargetMode(document.getElementById('server.moveOnSpam').checked);
}

function onPreInit(account, accountValues)
{
  buildServerFilterMenuList();
}

function updateMoveTargetMode(aEnable)
{
  if (aEnable)
    document.getElementById('broadcaster_moveMode').removeAttribute('disabled');
  else
    document.getElementById('broadcaster_moveMode').setAttribute('disabled', "true");    
}

function updateSpamLevel()
{
  document.getElementById('server.spamLevel').value =
    document.getElementById('spamLevel').checked ? 100 : 0;
}

// propagate changes to the server filter menu list back to 
// our hidden wsm element.
function onServerFilterListChange()
{
  document.getElementById('server.serverFilterName').value =
    document.getElementById("useServerFilterList").value;
}

// propagate changes to the whitelist menu list back to
// our hidden wsm element.
function onWhiteListAbURIChange()
{
  document.getElementById('server.whiteListAbURI').value =
    document.getElementById("whiteListAbURI").value;
}

function onActionTargetChange(aMenuList, aWSMElementId)
{
  document.getElementById(aWSMElementId).value = aMenuList.getAttribute('uri');
}

function buildServerFilterMenuList()
{
  // First, scan the profile directory for any .sfd files we may have there. 
  var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"]
                              .getService(Components.interfaces.nsIProperties);

  var profileDir = fileLocator.get(KEY_PROFILEDIR, Components.interfaces.nsIFile);
  buildServerFilterListFromDir(profileDir);

  // Then, fall back to defaults\messenger and list the default sfd files we shipped with
  var appDir = fileLocator.get(KEY_APPDIR, Components.interfaces.nsIFile);
  appDir.append('defaults');
  appDir.append('messenger');

  buildServerFilterListFromDir(appDir);
}

// helper function called by buildServerFilterMenuList. Enumerates over the passed in
// directory looking for .sfd files. For each entry found, it gets appended to the menu list
function buildServerFilterListFromDir(aDir)
{
  var ispHeaderList = document.getElementById('useServerFilterList');

  // now iterate over each file in the directory looking for .sfd files
  var entries = aDir.directoryEntries.QueryInterface(Components.interfaces.nsIDirectoryEnumerator);

  while (entries.hasMoreElements())
  {
    var entry = entries.nextFile;
    if (entry.isFile())
    {
      // we only care about files that end in .sfd
      if (entry.isFile() && /\.sfd$/.test(entry.leafName))
      {
        var fileName = RegExp.leftContext;
        // if we've already added an item with this name, then don't add it again.
        if (ispHeaderList.getElementsByAttribute("value", fileName).item(0))
          continue;
        ispHeaderList.appendItem(fileName, fileName);
      }
    }
  }
}
