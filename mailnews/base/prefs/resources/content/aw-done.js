/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

var gPrefsBundle;

function donePageInit() {
    var pageData = parent.GetPageData();
    var currentAccountData = gCurrentAccountData;

    if ("testingIspServices" in this) {
      if (testingIspServices()) {
        if ("setOtherISPServices" in this) {
          setOtherISPServices();
        }

        if (currentAccountData && currentAccountData.useOverlayPanels && currentAccountData.createNewAccount) {
          var backButton = document.documentElement.getButton("back");
          backButton.setAttribute("disabled", true);

          var cancelButton = document.documentElement.getButton("cancel");
          cancelButton.setAttribute("disabled", true);

          setPageData(pageData, "identity", "email", gEmailAddress);
          setPageData(pageData, "identity", "fullName", gUserFullName);
          setPageData(pageData, "login", "username", gScreenName);
        }
      }
    }
   
    gPrefsBundle = document.getElementById("bundle_prefs");
    var showMailServerDetails = true; 

    if (currentAccountData) {
        // find out if we need to hide server details
        showMailServerDetails = currentAccountData.showServerDetailsOnWizardSummary; 
        // Change the username field description to email field label in aw-identity
        setUserNameDescField(currentAccountData.emailIDFieldTitle);
    }

    // Find out if we need to hide details for incoming servers
    var hideIncoming = (gCurrentAccountData && gCurrentAccountData.wizardHideIncoming);

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
    if (pageData.login && pageData.login.username)
        userName = pageData.login.username.value;
    if (!userName && email)
        userName = getUsernameFromEmail(email);
    // Hide the "username" field if we don't want to show information
    // on the incoming server.
    setDivTextFromForm("server.username", hideIncoming ? null : userName);

    var smtpUserName="";
    if (pageData.login && pageData.login.smtpusername)
        smtpUserName = pageData.login.smtpusername.value;
    if (!smtpUserName && email)
        smtpUserName = getUsernameFromEmail(email);
    setDivTextFromForm("smtpServer.username", smtpUserName);

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
            accountName = gPrefsBundle.getFormattedString("accountName",
                                                          [prettyName,
                                                           userName]);
            // Set that to be the name in the pagedata 
            pageData.accname.prettyName.value = accountName;
        }
    }
    setDivTextFromForm("account.name", accountName);

    // Show mail servers (incoming&outgoing) details
    // based on current account data. ISP can set 
    // rdf value of literal showServerDetailsOnWizardSummary
    // to false to hide server details
    if (showMailServerDetails && !serverIsNntp(pageData)) {
        var incomingServerName="";
        if (pageData.server && pageData.server.hostname) {
            incomingServerName = pageData.server.hostname.value;
            if (!incomingServerName && 
                 currentAccountData && 
                 currentAccountData.incomingServer.hostname)
                incomingServerName = currentAccountData.incomingServer.hostName;
        }
        // Hide the incoming server name field if the user specified
        // wizardHideIncoming in the ISP defaults file
        setDivTextFromForm("server.name", hideIncoming ? null : incomingServerName);

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
          if (!smtpServerName && smtpServer && smtpServer.hostname)
              smtpServerName = smtpServer.hostname;
        }
        setDivTextFromForm("smtpServer.name", smtpServerName);
    }
    else {
        setDivTextFromForm("server.name", null);
        setDivTextFromForm("server.type", null);
        setDivTextFromForm("smtpServer.name", null);
    }

    if (serverIsNntp(pageData)) {
        var newsServerName="";
        if (pageData.newsserver && pageData.newsserver.hostname)
            newsServerName = pageData.newsserver.hostname.value;
        if (newsServerName) {
            // No need to show username for news account
            setDivTextFromForm("server.username", null);
        }
        setDivTextFromForm("newsServer.name", newsServerName);
    }
    else {
        setDivTextFromForm("newsServer.name", null);
    }

    var isPop = false;
    if (pageData.server && pageData.server.servertype) {
      isPop = (pageData.server.servertype.value == "pop3");
    }

    hideShowDownloadMsgsUI(isPop);
}

function hideShowDownloadMsgsUI(isPop)
{
  // only show the "download messages now" UI
  // if this is a pop account, we are online, and this was opened
  // from the 3 pane
  var downloadMsgs = document.getElementById("downloadMsgs");
  if (isPop) {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    if (!ioService.offline) {
      if ((window.opener.location.href == "chrome://messenger/content/messenger.xul") || (window.opener.location.href == "chrome://messenger/content/mail3PaneWindowVertLayout.xul")) {
        downloadMsgs.hidden = false;
        return;
      }
    }
  }
 
  // else hide it
  downloadMsgs.hidden = true;
}

function setDivTextFromForm(divid, value) {

    // collapse the row if the div has no value
    var div = document.getElementById(divid);
    if (!value) {
        div.setAttribute("collapsed","true");
        return;
    }
    else {
        div.removeAttribute("collapsed");
    }

    // otherwise fill in the .text element
    div = document.getElementById(divid+".text");
    if (!div) return;

    // set the value
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
