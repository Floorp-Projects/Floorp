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

var selectedServer = null; 
var brandBundle    = null;
var msgBundle      = null;

function OnInit()
{
    var title        = null;
    var titleElement = null;
    var brandName    = null;
    var acctType     = null;
    var acctName     = null;

    // Set the header for the page.
    // Title containts the brand name of the application and the account
    // type (mail/news) and the name of the account
    try {
        // Get title element from the document
        titleElement = document.getElementById("AccountCentralTitle");

        // Get the brand name
        brandBundle = srGetStrBundle("chrome://global/locale/brand.properties");
        brandName   = brandBundle.GetStringFromName("brandShortName"); 

        // Get the account type
        msgBundle      = srGetStrBundle("chrome://messenger/locale/messenger.properties");
        selectedServer = GetSelectedServer(); 
        var serverType = selectedServer.type; 
        if (serverType == "nntp")
            acctType = msgBundle.GetStringFromName("newsAcctType");
        else
            acctType = msgBundle.GetStringFromName("mailAcctType");

        // Get the account name
        acctName = GetSelectedMsgFolderName();

        title = msgBundle.GetStringFromName("acctCentralTitleFormat")
                         .replace(/%brandName%/, brandName)
                         .replace(/%accountType%/, acctType)
                         .replace(/%accountName%/, acctName);

        titleElement.setAttribute("value", title);

        // Display and collapse items presented to the user based on account type
        ArrangeAccountCentralItems(serverType);
    }
    catch(ex) {
        dump("Error -> " + ex + "\n");
    }
}
 
function ArrangeAccountCentralItems(serverType)
{
    if (serverType == "nntp")
        ShowOnlyNewsItems();
    else
        ShowOnlyMailItems();
}

// From the current folder tree, return the selected server
function GetSelectedServer()
{
    var folderURI = window.parent.GetSelectedFolderURI();
    var server = GetServer(folderURI);
    return server;
}

// From the current folder tree, return the name of the folder selected
function GetSelectedMsgFolderName()
{
    var folderURI = window.parent.GetSelectedFolderURI();
    var msgFolder = window.parent.GetMsgFolderFromURI(folderURI);
    return msgFolder.prettyName;
}

// Base AccountCentral page has items pertained to both mail and news. 
// For news, we collapse all unwanted mail items and display all news items
function ShowOnlyNewsItems()
{
    try {
        document.getElementById("ReadMessages").setAttribute("collapsed", "true");
        document.getElementById("CreateFilters").setAttribute("collapsed", "true");
    }
    catch(ex) {
        dump("Error -> " + ex + "\n");
    } 
    
}

// Base AccountCentral page has items pertained to both mail and news. 
// For mail, we collapse all unwanted news items and display all mail items
function ShowOnlyMailItems()
{

    try {
        document.getElementById("ReadMessages").removeAttribute("collapsed");
        document.getElementById("CreateFilters").removeAttribute("collapsed");
    }
    catch(ex) {
        dump("Error -> " + ex + "\n");
    } 
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
function ViewSettings()
{
    window.parent.MsgAccountManager();
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
    window.parent.MsgFilters();
} 
