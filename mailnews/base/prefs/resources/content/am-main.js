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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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

function onInit() 
{
  var accountName = document.getElementById("server.prettyName");
  var title = document.getElementById("am-main-title");
  var defaultTitle = title.getAttribute("defaultTitle");
  var titleValue;

  if(accountName.value)
    titleValue = defaultTitle+" - <"+accountName.value+">";
  else
    titleValue = defaultTitle;

  title.setAttribute("title",titleValue);
    
  setupSignatureItems(); 
}

function onPreInit(account, accountValues)
{
  loadSMTPServerList();
}

function manageIdentities()
{
  // We want to save the current identity information before bringing up the multiple identities
  // UI. This ensures that the changes are reflected in the identity list dialog
  // onSave();

  var account = parent.getCurrentAccount();
  if (!account)
    return;

  var accountName = document.getElementById("server.prettyName").value;

  var args = { account: account, accountName: accountName, result: false };

  // save the current identity settings so they show up correctly
  // if the user just changed them in the manage identities dialog
  var identity = account.defaultIdentity;
  saveIdentitySettings(identity);

  window.openDialog('am-identities-list.xul', 'identity', 'modal,titlebar,chrome', args);

  if (args.result) {
    // now re-initialize the default identity settings in case they changed
    identity = account.defaultIdentity; // refetch the default identity in case it changed
    initIdentityValues(identity);
  }
}
