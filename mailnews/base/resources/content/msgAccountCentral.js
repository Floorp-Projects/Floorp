/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

var selectedServer   = null; 
var nsPrefBranch = null;

function OnInit()
{
    var title        = null;
    var titleElement = null;
    var brandName    = null;
    var acctType     = null;
    var acctName     = null;
    var brandBundle;
    var messengerBundle;

    try {
      if (!nsPrefBranch) {
        var prefService = Components.classes["@mozilla.org/preferences-service;1"];
        prefService = prefService.getService();
        prefService = prefService.QueryInterface(Components.interfaces.nsIPrefService);

        nsPrefBranch = prefService.getBranch(null);
      }
    }
    catch (ex) {
      dump("error getting pref service. "+ex+"\n");
    }

    // Set the header for the page.
    // Title containts the brand name of the application and the account
    // type (mail/news) and the name of the account
    try {
        // Get title element from the document
        titleElement = document.getElementById("AccountCentralTitle");

        // Get the brand name
        brandBundle = document.getElementById("bundle_brand");
        brandName    = brandBundle.getString("brandShortName"); 

        // Get the account type
        messengerBundle = document.getElementById("bundle_messenger");
        selectedServer = GetSelectedServer(); 
        var serverType = selectedServer.type; 
        if (serverType == "nntp")
            acctType = messengerBundle.getString("newsAcctType");
        else
            acctType = messengerBundle.getString("mailAcctType");

        // Get the account name
        var msgFolder = GetSelectedMsgFolder();
        acctName = msgFolder.prettyName;

        title = messengerBundle.getFormattedString("acctCentralTitleFormat",
                                                   [brandName,
                                                    acctType,
                                                    acctName]);

        titleElement.setAttribute("value", title);

        // Display and collapse items presented to the user based on account type
        var protocolInfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + serverType];
        protocolInfo = protocolInfo.getService(Components.interfaces.nsIMsgProtocolInfo);
        ArrangeAccountCentralItems(selectedServer, protocolInfo, msgFolder);
    }
    catch(ex) {
        dump("Error -> " + ex + "\n");
    }
}
 
// Show items in the AccountCentral page depending on the capabilities
// of the given account
function ArrangeAccountCentralItems(server, protocolInfo, msgFolder)
{
    try {
        /***** Email header and items : Begin *****/

        // Read Messages
        var canGetMessages = protocolInfo.canGetMessages;
        SetItemDisplay("ReadMessages", canGetMessages);
    
        // Compose Messages link
        var showComposeMsgLink = protocolInfo.showComposeMsgLink;
        SetItemDisplay("ComposeMessage", showComposeMsgLink);
    
        var displayEmailHeader = canGetMessages || showComposeMsgLink;
        // Display Email header, only if any of the items are displayed
        SetItemDisplay("EmailHeader", displayEmailHeader);
    
        /***** Email header and items : End *****/

        /***** News header and items : Begin *****/

        // Subscribe to Newsgroups 
        var canSubscribe = (msgFolder.canSubscribe) && !(protocolInfo.canGetMessages);
        SetItemDisplay("SubscribeNewsgroups", canSubscribe);
    
        // Display News header, only if any of the items are displayed
        SetItemDisplay("NewsHeader", canSubscribe);
    
        /***** News header and items : End *****/
   
        // If neither of above sections exist, collapse section separators
        if (!(canSubscribe || displayEmailHeader)) { 
            CollapseSectionSeparators("MessagesSection.separator", false);
        } 

        /***** Accounts : Begin *****/

        var canShowCreateAccount = ! nsPrefBranch.prefIsLocked("mail.accountmanager.accounts");
        SetItemDisplay("CreateAccount", canShowCreateAccount);
          
        /***** Accounts : End *****/

        /***** Advanced Features header and items : Begin *****/

        // Search Messages
        var canSearchMessages = server.canSearchMessages;
        SetItemDisplay("SearchMessages", canSearchMessages);
    
        // Create Filters
        var canHaveFilters = server.canHaveFilters;
        SetItemDisplay("CreateFilters", canHaveFilters);

        // Offline Settings
        var supportsOffline = (server.offlineSupportLevel != 0);
        SetItemDisplay("OfflineSettings", supportsOffline);
    
        var displayAdvFeatures = canSearchMessages || canHaveFilters || supportsOffline;
        // Display Adv Features header, only if any of the items are displayed
        SetItemDisplay("AdvancedFeaturesHeader", displayAdvFeatures);
    
        /***** Advanced Featuers header and items : End *****/
    }
    catch (ex) {
        dump("Error is setting AccountCentral Items : " + ex + "\n");
    }
}

// Collapse the item if the item feature is not supported 
function SetItemDisplay(elemId, displayThisItem)
{
    if (!displayThisItem) {
        var elem = document.getElementById(elemId); 
        elem.setAttribute("collapsed", true);

        var separatorId = elemId + ".separator";
        var elemSeparator = document.getElementById(separatorId); 
        elemSeparator.setAttribute("collapsed", true);
    }
}

// Collapse section separators  
function CollapseSectionSeparators(separatorBaseId)
{
    for (var i=1; i <= 3; i++) {
        var separatorId = separatorBaseId + i;
        var separator = document.getElementById(separatorId);   
        separator.setAttribute("collapsed", true);
    }
}

// From the current folder tree, return the selected server
function GetSelectedServer()
{
    var folderURI = window.parent.GetSelectedFolderURI();
    var server = GetServer(folderURI);
    return server;
}

// From the current folder tree, return the selected folder
function GetSelectedMsgFolder()
{
    var folderURI = window.parent.GetSelectedFolderURI();
    var msgFolder = window.parent.GetMsgFolderFromURI(folderURI);
    return msgFolder;
}

// Open Inbox for selected server.
// If needed, open th twsity and select Inbox.    
function ReadMessages()
{
    try {
        window.parent.OpenInboxForServer(selectedServer);
    }
    catch(ex) {
        dump("Error -> " + ex + "\n");
    } 
} 

// Trigger composer for a new message
function ComposeAMessage(event)
{
    window.parent.MsgNewMessage();
} 

// Open AccountManager to view settings for a given account
// selectPage: the xul file name for the viewing page, 
// null for the account main page, other pages are
// 'am-server.xul', 'am-copies.xul', 'am-offline.xul', 
// 'am-addressing.xul','am-advanced.xul', 'am-smtp.xul'
function ViewSettings(selectPage)
{
    window.parent.MsgAccountManager(selectPage);
} 

// Open AccountWizard to create an account
function CreateNewAccount()
{
    window.parent.msgOpenAccountWizard();  
}

// Bring up search interface for selected account
function SearchMessages()
{
    window.parent.MsgSearchMessages();
} 

// Open filters window
function CreateMsgFilters()
{
    window.parent.MsgFilters(null);
} 

// Open Subscribe dialog
function SubscribeNewsgroups()
{
    window.parent.MsgSubscribe();
} 
