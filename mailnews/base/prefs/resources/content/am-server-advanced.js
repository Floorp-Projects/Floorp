/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Alec Flett <alecf@netscape.com>
 *   David Bienvenu <bienvenu@nventure.com>
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

// pull stuff out of window.arguments
var gServerSettings = window.arguments[0];

var serverList;

var gAccountManager;
var gFirstDeferredAccount;
// initialize the controls with the "gServerSettings" argument

var gControls;
function getControls()
{
  if (!gControls)
    gControls = document.getElementsByAttribute("amsa_persist", "true");
  return gControls;
}

function getLocalFoldersAccount()
{
  var localFoldersServer = gAccountManager.localFoldersServer;
  return gAccountManager.FindAccountForServer(localFoldersServer);
}

function onLoad()
{
  if (gServerSettings.serverType == "imap")
  {
    document.getElementById("tabbox").selectedTab = document.getElementById("imapTab");
    document.getElementById("pop3Tab").hidden = true;
    // don't hide panel - it hides all subsequent panels
  }
  else if (gServerSettings.serverType == "pop3")
  {
    var radioGroup = document.getElementById("folderStorage");
    document.getElementById("tabbox").selectedTab = document.getElementById("pop3Tab");
    document.getElementById("imapTab").hidden = true;
    // just hide the tab, don't hide panel - it hides all subsequent panels
    gAccountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
    gFirstDeferredAccount = gServerSettings.deferredToAccount;
    var localFoldersAccount = getLocalFoldersAccount();
    if (gFirstDeferredAccount.length)
    {
      var account = gAccountManager.getAccount(gFirstDeferredAccount);
      if (account)
      {
        var thisServer = account.incomingServer;
        SetFolderPicker(thisServer.serverURI, 'deferedServerFolderPicker');
      }
      if (gFirstDeferredAccount == localFoldersAccount.key)
      {
        radioGroup.selectedItem = document.getElementById("globalInbox");
        SetFolderPicker(localFoldersAccount.incomingServer.serverURI, 'deferedServerFolderPicker');
        updateInboxAccount(false, true);
      }
      else
      {
        radioGroup.selectedItem = document.getElementById("deferToServer");
        SetFolderPicker(account.incomingServer.serverURI, 'deferedServerFolderPicker');
        updateInboxAccount(true, true);
      }
    }
    else
    {
      radioGroup.selectedItem = document.getElementById("accountDirectory");

      // we should find out if there's another pop3/movemail server to defer to,
      // perhaps by checking the number of elements in the picker. For now, 
      // just use the local folders account
      SetFolderPicker(localFoldersAccount.incomingServer.serverURI, 'deferedServerFolderPicker');

      updateInboxAccount(false, false);

    }
  }
  else
  {
    document.getElementById("imapTab").hidden = true;
    document.getElementById("pop3Tab").hidden = true;
  }
  var controls = getControls();

  for (var i = 0; i < controls.length; i++)
  {
    var slot = controls[i].id;
    if (slot in gServerSettings)
    {
      if (controls[i].localName == "checkbox")
        controls[i].checked = gServerSettings[slot];
      else
        controls[i].value = gServerSettings[slot];
    }
  }
}

function onOk()
{
  // Handle account deferral settings for POP3 accounts.
  if (gServerSettings.serverType == "pop3")
  {
    var radioGroup = document.getElementById("folderStorage");
    var gPrefsBundle = document.getElementById("bundle_prefs");

    // if this account wasn't deferred, and is now...
    if (radioGroup.value != 1 && !gFirstDeferredAccount.length)
    {
      var confirmDeferAccount =
        gPrefsBundle.getString("confirmDeferAccount");

      var confirmTitle = gPrefsBundle.getString("confirmDeferAccountTitle");

      var promptService =
        Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
                 getService(Components.interfaces.nsIPromptService);
      if (!promptService ||
          !promptService.confirm(window, confirmTitle, confirmDeferAccount))
        return false;
    }
    switch (radioGroup.value)
    {
      case "0":
        gServerSettings['deferredToAccount'] = getLocalFoldersAccount().key;
        break;
      case "1":
        gServerSettings['deferredToAccount'] = "";
        break;
      case "2":
        picker = document.getElementById("deferedServerFolderPicker");
        var server = GetMsgFolderFromUri(picker.getAttribute("uri"), false).server;
        var account = gAccountManager.FindAccountForServer(server);
        gServerSettings['deferredToAccount'] = account.key;
        break;
    }
  }

  // Save the controls back to the "gServerSettings" array.
  var controls = getControls();
  for (var i = 0; i < controls.length; i++)
  {
    var slot = controls[i].id;
    if (slot in gServerSettings)
    {
      if (controls[i].localName == "checkbox")
        gServerSettings[slot] = controls[i].checked;
      else
        gServerSettings[slot] = controls[i].value;
    }
  }
}


// Set radio element choices and picker states
function updateInboxAccount(showPicker, showDeferGetNewMail, event)
{
    var picker = document.getElementById('deferedServerFolderPicker');
    if (showPicker)
    {
      picker.hidden = false;
      picker.removeAttribute("disabled");
    }
    else
    {
      picker.hidden = true;
    }
    var deferCheckbox = document.getElementById('deferGetNewMail');
    deferCheckbox.hidden = !showDeferGetNewMail;
}
