/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 1998-2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   alecf@netscape.com
 *   sspitzer@netscape.com
 *   racham@netscape.com
 *   hwaara@chello.se
 *   bienvenu@nventure.com
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

var gRedirectorType = "";
var gServer;

function onInit() 
{
    initServerType();

    setupBiffUI();
    setupMailOnServerUI();
    setupFixedUI();
    setupNotifyUI();
}

function onPreInit(account, accountValues)
{
    // Bug 134238
    // Make sure server.isSecure will be saved before server.port preference
    parent.getAccountValue(account, accountValues, "server", "isSecure", null, false);

    var type = parent.getAccountValue(account, accountValues, "server", "type", null, false);
    gRedirectorType = parent.getAccountValue(account, accountValues, "server", "redirectorType", null, false);
    hideShowControls(type);

    gServer = account.incomingServer;
    if(!(account.incomingServer.isSecureServer))
      document.getElementById("server.isSecure").setAttribute("hidden", "true");
    else
      document.getElementById("server.isSecure").removeAttribute("hidden");
    
    if(!account.incomingServer.canEmptyTrashOnExit)
    {
      document.getElementById("server.emptyTrashOnExit").setAttribute("hidden", "true");
      document.getElementById("imap.deleteModel.box").setAttribute("hidden", "true");
    }
    var hideButton = false;

    try {
      if (gRedirectorType) {
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
        var prefString = "mail.accountmanager." + gRedirectorType + ".hide_advanced_button";
        hideButton = prefs.getBoolPref(prefString);
      }
    }
    catch (ex) { }
    if (hideButton)
      document.getElementById("server.advancedbutton").setAttribute("hidden", "true");  
    else 
      document.getElementById("server.advancedbutton").removeAttribute("hidden");  
}

function initServerType() {
  var serverType = document.getElementById("server.type").getAttribute("value");
  
  var propertyName = "serverType-" + serverType;

  var messengerBundle = document.getElementById("bundle_messenger");
  var verboseName = messengerBundle.getString(propertyName);
  setDivText("servertype.verbose", verboseName);
 
  var isSecureSelected = document.getElementById("server.isSecure").checked;
  var protocolInfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + serverType].getService(Components.interfaces.nsIMsgProtocolInfo);
  document.getElementById("defaultPort").value = protocolInfo.getDefaultServerPort(isSecureSelected);
}

function setDivText(divname, value) {
    var div = document.getElementById(divname);
    if (!div) return;
    div.setAttribute("value", value);
}


function onAdvanced()
{
  dump("onAdvanced..\n");
  var serverKeyElement = document.getElementById("identity.smtpServerKey");
  var oldSmtpServerKey = serverKeyElement.getAttribute("value");
  dump("selected key = " + oldSmtpServerKey + "\n");

  var serverSettings = {};
  serverSettings.smtpServerList = oldSmtpServerKey;

  // Store the server type and, if an IMAP or POP3 server,
  // the settings needed for the IMAP/POP3 tab into the array
  var serverType = document.getElementById("server.type").getAttribute("value");
  serverSettings.serverType = serverType;

  if (serverType == "imap")
  {
    serverSettings.dualUseFolders = document.getElementById("imap.dualUseFolders").checked;
    serverSettings.usingSubscription = document.getElementById("imap.usingSubscription").checked;
    serverSettings.useIdle = document.getElementById("imap.useIdle").checked;
    serverSettings.maximumConnectionsNumber = document.getElementById("imap.maximumConnectionsNumber").getAttribute("value");
    // string prefs
    serverSettings.personalNamespace = document.getElementById("imap.personalNamespace").getAttribute("value");
    serverSettings.publicNamespace = document.getElementById("imap.publicNamespace").getAttribute("value");
    serverSettings.serverDirectory = document.getElementById("imap.serverDirectory").getAttribute("value");
    serverSettings.otherUsersNamespace = document.getElementById("imap.otherUsersNamespace").getAttribute("value");
    serverSettings.overrideNamespaces = document.getElementById("imap.overrideNamespaces").checked;
  }
  else if (serverType == "pop3")
  {
    serverSettings.deferGetNewMail = document.getElementById("pop3.deferGetNewMail").checked;
    serverSettings.deferredToAccount = document.getElementById("pop3.deferredToAccount").getAttribute("value");
  }

  window.openDialog("chrome://messenger/content/am-server-advanced.xul",
                    "_blank", "chrome,modal,titlebar", serverSettings);

  if (serverSettings.smtpServerList != oldSmtpServerKey)
  {
    // save the identity back to the page as a key
    dump("Setting the smtp server to " + serverSettings.smtpServerList + "\n");
    if (serverSettings.smtpServerList)
      serverKeyElement.setAttribute("value", serverSettings.smtpServerList);
    else
      serverKeyElement.removeAttribute("value");
  }
  if (serverType == "imap")
  {
    document.getElementById("imap.dualUseFolders").checked = serverSettings.dualUseFolders;
    document.getElementById("imap.usingSubscription").checked = serverSettings.usingSubscription;
    document.getElementById("imap.useIdle").checked = serverSettings.useIdle;
    document.getElementById("imap.maximumConnectionsNumber").setAttribute("value", serverSettings.maximumConnectionsNumber);
    // string prefs
    document.getElementById("imap.personalNamespace").setAttribute("value", serverSettings.personalNamespace);
    document.getElementById("imap.publicNamespace").setAttribute("value", serverSettings.publicNamespace);
    document.getElementById("imap.serverDirectory").setAttribute("value", serverSettings.serverDirectory);
    document.getElementById("imap.otherUsersNamespace").setAttribute("value", serverSettings.otherUsersNamespace);
    document.getElementById("imap.overrideNamespaces").checked = serverSettings.overrideNamespaces;
  }
  else if (serverType == "pop3")
  {
    document.getElementById("pop3.deferGetNewMail").checked = serverSettings.deferGetNewMail;
    document.getElementById("pop3.deferredToAccount").setAttribute("value", serverSettings.deferredToAccount);
    var pop3Server = gServer.QueryInterface(Components.interfaces.nsIPop3IncomingServer);
    // we're explicitly setting this so we'll go through the SetDeferredToAccount method
    pop3Server.deferredToAccount = serverSettings.deferredToAccount;
  }
}

function secureSelect() {
    var serverType   = document.getElementById("server.type").getAttribute("value");
    var protocolInfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + serverType].getService(Components.interfaces.nsIMsgProtocolInfo);
    var isSecureSelected = document.getElementById("server.isSecure").checked;
    var defaultPort = protocolInfo.getDefaultServerPort(false);
    var defaultPortSecure = protocolInfo.getDefaultServerPort(true);
    var previouslyDisplayedPort = document.getElementById("server.port").value;

    if (isSecureSelected) {
      document.getElementById("defaultPort").value = defaultPortSecure;
      if (previouslyDisplayedPort == defaultPort)
        document.getElementById("server.port").value = defaultPortSecure;
    }
    else {
      document.getElementById("defaultPort").value = defaultPort;
      if (previouslyDisplayedPort == defaultPortSecure)
        document.getElementById("server.port").value = defaultPort;
    } 
}

function setupBiffUI()
{ 
   var broadcaster = document.getElementById("broadcaster_doBiff");

   var dobiff = document.getElementById("server.doBiff");
   var checked = dobiff.checked;
   var locked = getAccountValueIsLocked(dobiff);

   if (checked)
     broadcaster.removeAttribute("disabled");
   else
     broadcaster.setAttribute("disabled", "true");
   if (locked)
     broadcaster.setAttribute("disabled","true");
}

function setupMailOnServerUI()
{ 
   var checked = document.getElementById("pop3.leaveMessagesOnServer").checked;
   var locked = getAccountValueIsLocked(document.getElementById("pop3.leaveMessagesOnServer"));
   document.getElementById("pop3.deleteMailLeftOnServer").disabled = locked || !checked ;
   setupAgeMsgOnServerUI();
}

function setupAgeMsgOnServerUI()
{ 
   var leaveMsgsChecked = document.getElementById("pop3.leaveMessagesOnServer").checked;
   var checked = document.getElementById("pop3.deleteByAgeFromServer").checked;
   var locked = getAccountValueIsLocked(document.getElementById("pop3.deleteByAgeFromServer"));
   document.getElementById("pop3.deleteByAgeFromServer").disabled = locked || !leaveMsgsChecked;
   document.getElementById("daysEnd").disabled = locked || !leaveMsgsChecked;
   document.getElementById("pop3.numDaysToLeaveOnServer").disabled = locked || !checked || !leaveMsgsChecked;
}

function setupFixedUI()
{
  // if the redirectorType is non-empty, then the fixedValue elements should be shown
  //
  // one day, add code that allows for the redirector type to specify
  // to show fixed values or not.  see bug #132737
  // use gRedirectorType to get a pref like
  // "mail.accountmanager." + gRedirectorType + ".show_fixed_values"
  //
  // but for now, this isn't needed.  we'll assume those who implement
  // redirector types want to show fixed values.
  var showFixedValues = gRedirectorType ? true : false;

  var controls = [document.getElementById("fixedServerName"), 
                  document.getElementById("fixedUserName"),
                  document.getElementById("fixedServerPort")];

  var len = controls.length;  
  for (var i=0; i<len; i++) {
    var fixedElement = controls[i];
    var otherElement = document.getElementById(fixedElement.getAttribute("use"));

    if (showFixedValues) {
      // get the value from the non-fixed element.
      var fixedElementValue = otherElement.value;
      fixedElement.setAttribute("value", fixedElementValue);

      otherElement.setAttribute("collapsed","true");
      fixedElement.removeAttribute("collapsed");
    }
    else {
      fixedElement.setAttribute("collapsed","true");
      otherElement.removeAttribute("collapsed");
    }
  }
}

function setupNotifyUI()
{ 
    var broadcaster = document.getElementById("broadcaster_notify");

    var notify = document.getElementById("nntp.notifyOn");
    var checked = notify.checked;
    var locked = getAccountValueIsLocked(notify);

    if (checked)
      broadcaster.removeAttribute("disabled");
    else
      broadcaster.setAttribute("disabled", "true");
    if (locked)
      broadcaster.setAttribute("disabled","true");
}

function BrowseForNewsrc()
{
  var newsrcTextBox = document.getElementById("nntp.newsrcFilePath");
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, 
          document.getElementById("browseForNewsrc").getAttribute("filepickertitle"),
          nsIFilePicker.modeSave);

  var currentNewsrcFile;
  try {
    currentNewsrcFile = Components.classes[LOCALFILE_CTRID].createInstance(nsILocalFile);
    currentNewsrcFile.initWithPath(newsrcTextBox.value);
  } catch (e) {
    dump("Failed to create nsILocalFile instance for the current newsrc file.\n");
  }

  if (currentNewsrcFile) {
    fp.displayDirectory = currentNewsrcFile.parent;
    fp.defaultString = currentNewsrcFile.leafName;
  }

  fp.appendFilters(nsIFilePicker.filterAll);

  if (fp.show() != nsIFilePicker.returnCancel)
    newsrcTextBox.value = fp.file.path;
}
