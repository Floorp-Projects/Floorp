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
 * The Original Code is Mozilla Mail Code.
 *
 * The Initial Developer of the Original Code is
 * Scott MacGregor.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@mozilla.org>
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

var gIdentityListBox;                 // the root <listbox> node
var gAddButton;
var gEditButton;
var gDeleteButton;
var gMessengerBundle;

var gAccount = null;  // the account we are showing the identities for

function onLoad()
{
  gMessengerBundle = document.getElementById("bundle_messenger");

  gIdentityListBox = document.getElementById("identitiesList");
  gAddButton        = document.getElementById("addButton");
  gEditButton       = document.getElementById("editButton");
  gDeleteButton     = document.getElementById("deleteButton");

  // extract the account
  gAccount = window.arguments[0].account;

  var accountName = window.arguments[0].accountName;
  document.title = document.getElementById("bundle_prefs")
                           .getFormattedString("identity-list-title", [accountName]);

  // extract the account from 
  refreshIdentityList();

  // try selecting the first identity
  gIdentityListBox.selectedIndex = 0;
}

function refreshIdentityList()
{
  // remove all children
  while (gIdentityListBox.hasChildNodes())
    gIdentityListBox.removeChild(gIdentityListBox.lastChild);

  var identities = gAccount.identities;
  var identitiesCount = identities.Count();
  for (var j = 0; j < identitiesCount; j++) 
  {
    var identity = identities.QueryElementAt(j, Components.interfaces.nsIMsgIdentity);
    if (identity.valid) 
    {
      var listitem = document.createElement("listitem");
      listitem.setAttribute("label", identity.identityName);
      listitem.setAttribute("key", identity.key);
      gIdentityListBox.appendChild(listitem);
    }
  }
}

// opens the identity editor dialog
// identity: pass in the identity (if any) to load in the dialog
function openIdentityEditor(identity)
{
  var result = false; 
  var args = { identity: identity, account: gAccount, result: result };

  window.openDialog('am-identity-edit.xul', '', 'modal,titlebar,chrome', args);

  var selectedItemIndex = gIdentityListBox.selectedIndex;

  if (args.result)
  {
    refreshIdentityList();   
    gIdentityListBox.selectedIndex = selectedItemIndex;
  }
}

function getSelectedIdentity()
{
  var identityKey = gIdentityListBox.selectedItems[0].getAttribute("key");
  var identities = gAccount.identities;
  var identitiesCount = identities.Count();
	for (var j = 0; j < identitiesCount; j++) 
  {
    var identity = identities.QueryElementAt(j, Components.interfaces.nsIMsgIdentity);
    if (identity.valid && identity.key == identityKey) 
      return identity;
  }

  return null; // no identity found
}

function onEdit(event)
{
  var id = (event.target.localName == 'listbox') ? null : getSelectedIdentity();
  openIdentityEditor(id);
}

function updateButtons()
{
  if (gIdentityListBox.selectedItems.length <= 0) 
  {
    gEditButton.setAttribute("disabled", "true");
    gDeleteButton.setAttribute("disabled", "true");
  } 
  else 
  {
    gEditButton.removeAttribute("disabled");
    if (gIdentityListBox.getRowCount() > 1)
      gDeleteButton.removeAttribute("disabled");
  }
}

function onDelete(event)
{
  if (gIdentityListBox.getRowCount() <= 1)  // don't support deleting the last identity
    return;

  gAccount.removeIdentity(getSelectedIdentity());
  // rebuild the list
  refreshIdentityList(); 
}

function onOk()
{
  window.arguments[0].result = true;
  return true;
}

function onCancel()
{
  return true;
}

function onSetDefault(event)
{
  // not implemented yet
}
