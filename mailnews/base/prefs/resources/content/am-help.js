/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/**
 * Key value pairs to derive the tag based on the page loaded.
 * Each key is the page loaded when user clicks on one of the items on
 * the accounttree of the AccountManager window.
 * Value is a tag that is preset which will be used to display
 * context sensitive help. 
 */
var pageTagPairs = {
  "chrome://messenger/content/am-main.xul": "mail_account_identity",
  "chrome://messenger/content/am-server.xul": "mail",
  "chrome://messenger/content/am-copies.xul": "mail_copies",
  "chrome://messenger/content/am-addressing.xul": "mail_addressing_settings",
  "chrome://messenger/content/am-offline.xul": "mail-offline-accounts",
  "chrome://messenger/content/am-smtp.xul": "mail_smtp",
  "chrome://messenger/content/am-smime.xul": "mail_security_settings",
  "chrome://messenger/content/am-serverwithnoidentities.xul": "mail_local_folders_settings",
  "chrome://messenger/content/am-mdn.xul": "mail-account-receipts",
  "chrome://messenger/content/am-fakeaccount.xul": "fake_account"
} 

function doHelpButton() 
{
  // Get the URI of the page loaded in the AccountManager's content frame.
  var pageSourceURI = contentFrame.location.href;
  // Get the help tag corresponding to the page loaded.
  var helpTag = pageTagPairs[pageSourceURI];

  // If the help tag is generic or offline, check if there is a need to set tags per server type
  if ((helpTag == "mail") || (helpTag == "mail-offline-accounts")) {
    // Get server type, as we may need to set help tags per server type for some pages
    var serverType = GetServerType();
  
    /**
     * Check the page to be loaded. Following pages needed to be presented with the 
     * help content that is based on server type. For any pages with such requirement
     * do add comments here about the page and a new case statement for pageSourceURI
     * switch.
     * - server settings ("chrome://messenger/content/am-server.xul")
     * - offline/diskspace settings ("chrome://messenger/content/am-offline.xul")
     */ 
    switch (pageSourceURI) {
      case "chrome://messenger/content/am-server.xul":
        helpTag = "mail_server_" + serverType;
        break;

      case "chrome://messenger/content/am-offline.xul":
        helpTag = "mail_offline_" + serverType;
        break;

      default :
        break;
    }
  }

  if ( helpTag ) 
  	openHelp(helpTag);  
  else
	openHelp('mail'); 
}

/**
 * Get server type of the seleted item
 */
function GetServerType()
{
  var serverType = null;
  var idStruct = getServerIdAndPageIdFromTree(accounttree);
  if (idStruct) {
    var account = getAccountFromServerId(idStruct.serverId);
    if (account) {
      serverType = account.incomingServer.type;
    }
  }
  return serverType;
}
