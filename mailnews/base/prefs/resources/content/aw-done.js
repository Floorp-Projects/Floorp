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

var Bundle = srGetStrBundle("chrome://messenger/locale/prefs.properties");

function onInit() {
    var pageData = parent.wizardManager.WSM.PageData;
    var showMailServerDetails = true; 

    var currentAccountData = parent.gCurrentAccountData;
    if (currentAccountData) {
        // find out if we need to hide server details
        showMailServerDetails = currentAccountData.showServerDetailsOnWizardSummary; 
  
        // Change the username field description to email field label in aw-identity
        setUserNameDescField(currentAccountData.emailIDFieldTitle);
    }

    var email = "";
    if (pageData.identity && pageData.identity.email) {
        // fixup the email
        email = pageData.identity.email.value;
        if (email.split('@').length < 2 && 
                     currentAccountData && 
                     currentAccountData.domain)
            email += "@" + currentAccountData.domain;
    }
    setDivTextFromForm("identity.email", email);

    var userName="";
    if (pageData.login && pageData.login.username) {
        userName = pageData.login.username.value;
    }
    if (!userName && email) {
        var emailData = email.split('@');
        userName = emailData[0];
    }
    setDivTextFromForm("server.username", userName);

    var accountName="";
    if (pageData.accname && pageData.accname.prettyName) {
        accountName = pageData.accname.prettyName.value;

        // If the AccountData exists, tha means we have values read from rdf file.
        // Get the pretty name and polish the account name
        if ( currentAccountData && 
             currentAccountData.incomingServer.prettyName)
        {
            var prettyName = currentAccountData.incomingServer.prettyName; 
            // Get the polished account name 
            accountName = Bundle.GetStringFromName("accountName")
                                .replace(/%prettyName%/, prettyName)
                                .replace(/%username%/, userName);  

            // Set that to be the name in the pagedata 
            pageData.accname.prettyName.value = accountName;
        }
    }
    setDivTextFromForm("account.name", accountName);

    // Show mail servers (incoming&outgoing) detials
    // based on current account data. ISP can set 
    // rdf value of literal showServerDetailsOnWizardSummary
    // to false to hide server details
    if (showMailServerDetails) {
        var incomingServerName="";
        if (pageData.server && pageData.server.hostname) {
            incomingServerName = pageData.server.hostname.value;
            if (!incomingServerName && 
                 currentAccountData && 
                 currentAccountData.incomingServer.hostname)
                incomingServerName = currentAccountData.incomingServer.hostName;
        }
        setDivTextFromForm("server.name", incomingServerName);

        var incomingServerType="";
        if (pageData.server && pageData.server.servertype) {
            incomingServerType = pageData.server.servertype.value;
            if (!incomingServerType && 
                 currentAccountData && 
                 currentAccountData.incomingServer.type)
                incomingServerType = currentAccountData.incomingServer.type;
        }
        setDivTextFromForm("server.type", incomingServerType.toUpperCase());

        var smtpServerName="";
        if (pageData.server && pageData.server.smtphostname) {
            var smtpServer = parent.smtpService.defaultServer;
            smtpServerName = pageData.server.smtphostname.value;
            if (!smtpServerName && smtpServer.hostname)
                smtpServerName = smtpServer.hostname;
        }
        setDivTextFromForm("smtpServer.name", smtpServerName);
    }
    else {
        setDivTextFromForm("server.name", null);
        setDivTextFromForm("server.type", null);
        setDivTextFromForm("smtpServer.name", null);
    }

    var newsServerName="";
    if (pageData.newsserver && pageData.newsserver.hostname)
        newsServerName = pageData.newsserver.hostname.value;
    if (newsServerName) {
        // No need to show username for news account
        setDivTextFromForm("server.username", null);
    }
    setDivTextFromForm("newsServer.name", newsServerName);
}

function setDivTextFromForm(divid, value) {

    // collapse the row if the div has no value
    if (!value) {
        var div = document.getElementById(divid);
        div.setAttribute("collapsed","true");
        return;
    }

    // otherwise fill in the .text element
    div = document.getElementById(divid+".text");
    if (!div) return;

    div.setAttribute("value", value);
}

function setUserNameDescField(name)
{
   if (name) {
       var userNameField = document.getElementById("server.username.label");
       userNameField.setAttribute("value", name);
   }
}

function setupAnother(event)
{
    window.alert("Unimplemented, see bug #19982");
}
