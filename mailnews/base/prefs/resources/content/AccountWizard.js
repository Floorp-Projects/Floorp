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

var pagePrefix="aw-";
var pagePostfix=".xul";

var currentPageTag;

var contentWindow;

var wizardContents;

function init() {
    if (!contentWindow) contentWindow = window.frames["wizardContents"];
    if (!wizardContents) wizardContents = new Array;
}
// event handlers
function onLoad() {
    init();
}

function wizardPageLoaded(tag) {
    init();
    currentPageTag=tag;
    initializePage(contentWindow, wizardContents);
}

function onNext(event) {

    if (!wizardMap[currentPageTag]) {
        dump("Error, could not find entry for current page: " +
             currentPageTag + "\n");
        return;
    }
    
    // only run validation routine if it's there
    var validate = wizardMap[currentPageTag].validate;
    if (validate)
        if (!validate(contentWindow, wizardContents)) return;

    if (typeof(contentWindow.ValidateContents) == "function")
        if (!contentWindow.ValidateContents()) return;

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


// helper functions that actually do stuff
function setNextEnabled(enabled) {
    
}

function setBackEnabled(enabled) {

}

function nextPage(win) {
    var nextPageTag = wizardMap[currentPageTag].next;
    dump("Loading " + getUrlFromTag(nextPageTag) + "\n");
    if (nextPageTag)
        win.location=getUrlFromTag(nextPageTag);
}

function previousPage(win) {
    previousPageTag = wizardMap[currentPageTag].previous;
    if (previousPageTag)
        win.location=getUrlFromTag(previousPageTag)
}

function initializePage(win, hash) {
    var doc = win.document;
    for (var i in hash) {
        var formElement=doc.getElementById(i);
        if (formElement) {
            formElement.value = hash[i];
        }

    }
}


function saveContents(win, hash) {

    var inputs = win.document.getElementsByTagName("FORM")[0].elements;
    dump("There are " + inputs.length + " input tags\n");
    for (var i=0 ; i<inputs.length; i++) {
        dump("Saving: ");
        dump("ID=" + inputs[i].name + " Value=" + inputs[i].value + "..");
        hash[inputs[i].name] = inputs[i].value;
        dump("done.\n");
    }

}

function validateIdentity(win, hash) {
    var email = win.document.getElementById("email").value;
    
    if (email.indexOf('@') == -1) {
        window.alert("Invalid e-mail address!");
        dump("Invalid e-mail address!\n");
        return false;
    }
    return true;
}

function validateServer(win, hash) {
    return true;
}

function validateSmtp(win, hash) {
    return true;
}

