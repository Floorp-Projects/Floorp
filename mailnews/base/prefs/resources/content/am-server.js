/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

var stringBundle;

function onInit() {
    stringBundle = srGetStrBundle("chrome://messenger/locale/messenger.properties");
    initServerType();
}

function onPreInit(account, accountValues)
{
    var type = parent.getAccountValue(account, accountValues, "server", "type");
    
    hideShowControls(type);
}


function initServerType() {
  var serverType = document.getElementById("server.type").getAttribute("value");
  
  var verboseName;

  var propertyName = "serverType-" + serverType;

  verboseName = stringBundle.GetStringFromName(propertyName);

  var hostname = document.getElementById("server.hostName").getAttribute("value");
  var username = document.getElementById("server.username").getAttribute("value");

  setDivText("servertype.verbose", verboseName);
  setDivText("servername.verbose", hostname);
  setDivText("username.verbose", username);

}

function hideShowControls(serverType)
{
    var controls = document.getElementsByAttribute("wsm_persist", "true");
    var len = controls.length;
    for (var i=0; i<len; i++) {
        var control = controls[i];
        var controlName = control.id;
        if (!controlName) continue;
        var controlNameSplit = controlName.split(".");
        
        if (controlNameSplit.length < 2) continue;
        var controlType = controlNameSplit[0];

        // skip generic server/identity things
        var hideFor = control.getAttribute("hidefor");
        if ((!hideFor || hideFor == "") &&
            (controlType == "server" ||
             controlType == "identity")) continue;

        var box = getEnclosingContainer(control);

        if (!box) continue;

        // hide unsupported server type

        // adding support for hiding multiple server types using hideFor="server1,server2"
        var hideForBool = false;
        var hideForTokens = hideFor.split(",");
        for (j = 0; j < hideForTokens.length; j++) {
            if (hideForTokens[j] == serverType) {
                hideForBool = true;
                break;
            }
        }

        if ((controlType != "server" &&
             controlType != "identity" &&
             controlType != serverType) ||
            hideForBool) {
            box.setAttribute("hidden", "true");
        }
        else {
            box.removeAttribute("hidden");
        }
    }

    var serverPrefContainer = document.getElementById("serverPrefContainer");
    if (serverPrefContainer)
        serverPrefContainer.removeAttribute("hidden");
}


function setDivText(divname, value) {
    var div = document.getElementById(divname);
    if (!div) return;
    div.setAttribute("value", value);
}


function openImapAdvanced()
{
    var imapServer = getImapServer();
    dump("Opening dialog..\n");
    window.openDialog("chrome://messenger/content/am-imap-advanced.xul",
                      "_blank",
                      "chrome,modal", imapServer);

    saveServerLocally(imapServer);
}

function getImapServer() {
    var imapServer = new Array;

    imapServer.dualUseFolders = document.getElementById("imap.dualUseFolders").checked

    imapServer.usingSubscription = document.getElementById("imap.usingSubscription").checked;

    // string prefs
    imapServer.personalNamespace = document.getElementById("imap.personalNamespace").getAttribute("value");
    imapServer.publicNamespace = document.getElementById("imap.publicNamespace").getAttribute("value");
    imapServer.serverDirectory = document.getElementById("imap.serverDirectory").getAttribute("value");
    imapServer.otherUsersNamespace = document.getElementById("imap.otherUsersNamespace").getAttribute("value");

    imapServer.overrideNamespaces = document.getElementById("imap.overrideNamespaces").checked;
    return imapServer;
}

function saveServerLocally(imapServer)
{
    document.getElementById("imap.dualUseFolders").checked = imapServer.dualUseFolders;
    document.getElementById("imap.usingSubscription").checked = imapServer.usingSubscription;

    // string prefs
    document.getElementById("imap.personalNamespace").setAttribute("value", imapServer.personalNamespace);
    document.getElementById("imap.publicNamespace").setAttribute("value", imapServer.publicNamespace);
    document.getElementById("imap.serverDirectory").setAttribute("value", imapServer.serverDirectory);
    document.getElementById("imap.otherUsersNamespace").setAttribute("value", imapServer.otherUsersNamespace);

    document.getElementById("imap.overrideNamespaces").checked = imapServer.overrideNamespaces;

}

function getEnclosingContainer(startNode) {

    var parent = startNode;
    var box;
    
    while (parent && parent != document) {

        var isContainer =
            (parent.getAttribute("iscontrolcontainer") == "true");
          
        // remember the FIRST container we encounter, or the first
        // controlcontainer
        if (!box || isContainer)
            box=parent;
        
        // break out with a controlcontainer
        if (isContainer)
            break;
        parent = parent.parentNode;
    }
    
    return box;
}
