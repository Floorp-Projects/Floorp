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

var wizardMap = {
    accounttype: { next: "identity" },
    identity: { next: "server", previous: "accounttype" },
    server:   { next: "login", previous: "identity"},
    login:    { next: "accname", previous: "server"},
    accname:  { next: "done", previous: "login" },
    done:     { previous: "accname" }
}

var pagePrefix="chrome://messenger/content/aw-";
var pagePostfix=".xul";

var currentPageTag;

var contentWindow;

var wizardContents;
var smtpService;

function init() {
    if (!contentWindow) contentWindow = window.frames["wizardContents"];
    if (!wizardContents) wizardContents = new Array;
}
// event handlers
function onLoad() {
    if (!smtpService)
        smtpService =
            Components.classes["component://netscape/messengercompose/smtp"].getService(Components.interfaces.nsISmtpService);;

    init();
    try {
        wizardContents["smtp.hostname"] = smtpService.defaultServer.hostname;
        dump("initialized with " + wizardContents["smtp.hostname"] + "\n");
    }
    catch (ex) {
        dump("failed to get the smtp hostname:" + ex + "\n");
    }
}

function wizardPageLoaded(tag) {
    init();
    currentPageTag=tag;
    initializePage(contentWindow, wizardContents);
    updateButtons(wizardMap[currentPageTag], window.parent);
}

function onNext(event) {

    if (!wizardMap[currentPageTag]) {
        dump("Error, could not find entry for current page: " +
             currentPageTag + "\n");
        return;
    }
    
    // only run validation routine if it's there
    if (contentWindow.validate)
        if (!contentWindow.validate(wizardContents)) return;

    saveContents(contentWindow, wizardContents);
    
    nextPage(contentWindow);

}

function onCancel(event) {
    window.close();
}

function onLoadPage(event) {
    contentWindow.location = getUrlFromTag(document.getElementById("newPage").value);
}

function onBack(event) {
    previousPage(contentWindow);
}

// utility functions
function getUrlFromTag(title) {
    return pagePrefix + title + pagePostfix;
}


function setNextText(doc, hasNext) {
    if (hasNext)
        doc.getElementById("nextButton").setAttribute("value", "Next >");
    else
        doc.getElementById("nextButton").setAttribute("value", "Finish");

}
function setNextEnabled(doc, enabled) {
    if (enabled)
        doc.getElementById("nextButton").removeAttribute("disabled");
    else
        doc.getElementById("nextButton").setAttribute("disabled", "true");
}

function setBackEnabled(doc, enabled) {
    if (enabled)
        doc.getElementById("backButton").removeAttribute("disabled");
    else
        doc.getElementById("backButton").setAttribute("disabled", "true");
}

function nextPage(win) {
    var nextPageTag = wizardMap[currentPageTag].next;
    if (nextPageTag)
        win.location=getUrlFromTag(nextPageTag);
    else
        onFinish();
}

function previousPage(win) {
    previousPageTag = wizardMap[currentPageTag].previous;
    if (previousPageTag)
        win.location=getUrlFromTag(previousPageTag)
}

function initializePage(win, hash) {
    var inputs= win.document.getElementsByTagName("FORM")[0].elements;
    for (var i=0; i<inputs.length; i++) {
        restoreValue(hash, inputs[i]);
    }

    if (win.onInit) win.onInit();
}


function updateButtons(mapEntry, wizardWindow) {

    //setNextEnabled(wizardWindow.document, mapEntry.next ? true : false);
    setNextText(wizardWindow.document, mapEntry.next ? true : false);
    setBackEnabled(wizardWindow.document, mapEntry.previous ? true : false);
}

function saveContents(win, hash) {

    var inputs = win.document.getElementsByTagName("FORM")[0].elements;
    for (var i=0 ; i<inputs.length; i++) {
        saveValue(hash, inputs[i])
   }

}

function restoreValue(hash, element) {
    if (!hash[element.name]) return;
    if (element.type=="radio") {
        if (hash[element.name] == element.value)
            element.checked=true;
        else
            element.checked=false;
    } else if (element.type=="checkbox") {
        element.checked=hash[element.name];
    } else {
        element.value=hash[element.name];
    }

}

function saveValue(hash, element) {

    if (element.type=="radio") {
        if (element.checked) {
            hash[element.name] = element.value;
        }
    } else if (element.type == "checkbox") {
        hash[element.name] = element.checked;
    }
    else {
        hash[element.name] = element.value;
    }

}

function onFinish() {
    var i;
    dump("There are " + wizardContents.length + " elements\n");
    for (i in wizardContents) {
        dump("wizardContents[" + i + "] = " + wizardContents[i] + "\n");
    }
    if (createAccount(wizardContents))
        window.arguments[0].refresh = true;

    // hack hack - save the prefs file NOW in case we crash
    try {
        var prefs = Components.classes["component://netscape/preferences"].getService(Components.interfaces.nsIPref);
        prefs.SavePrefFile();
    } catch (ex) {
        dump("Error saving prefs!\n");
    }
    window.close();
}

function createAccount(hash) {

    try {
        var am = Components.classes["component://netscape/messenger/account-manager"].getService(Components.interfaces.nsIMsgAccountManager);


    // workaround for lame-ass combo box bug
    var serverType = hash["server.type"];
    if (!serverType  || serverType == "")
        serverType = "pop3";
    var server = am.createIncomingServer(serverType);
    
    var identity = am.createIdentity();

    for (var i in hash) {
        var vals = i.split(".");
        var type = vals[0];
        var slot = vals[1];

        if (type == "identity")
            identity[slot] = hash[i];
        else if (type == "server")
            server[slot] = hash[i];
        else if (type == "smtp")
            smtpService.defaultServer.hostname = hash[i];
    }
    
    /* new nntp identities should use plain text by default
     * we wan't that GNKSA (The Good Net-Keeping Seal of Approval) */
    if (type == "nntp") {
	identity.composeHtml = false;
    }

    var account = am.createAccount();
    account.incomingServer = server;
    account.addIdentity(identity);

    // check if there already is a "none" account. (aka "Local Folders")
    // if not, create it.
	var localMailServer = null;
	try {
		// look for anything that is of type "none".
		// "none" is the type for "Local Mail"
		localMailServer = am.findServer("","","none");
	}
	catch (ex) {
		localMailServer = null;
	}

	if (!localMailServer) {
		// creates a copy of the identity you pass in
		am.createLocalMailAccount(identity, false);
        }

    return true;
    
    } catch (ex) {
        // return false (meaning we did not create the account)
        // on any error
        dump("Error creating account: ex\n");
        return false;
    }
}

