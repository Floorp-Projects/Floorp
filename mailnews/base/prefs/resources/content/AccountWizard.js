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
    intro:    { next: "identity"},
    identity: { next: "smtp",   previous: "intro"},
    smtp:     { next: "server", previous: "identity"},
    server:   { next: "done", previous: "server"},
    done:     { previous: "server" }
}

var pagePrefix="aw-";
var pagePostfix=".xul";

var currentPageTag;

var contentWindow;

// event handlers
function onLoad() {
    dump("Initializing the wizard..\n");
    contentWindow = window.frames["wizardContents"];
}

function onNext(event) {
    savePageInfo(contentWindow);
    nextPage(contentWindow);
    initializePage(contentWindow);
}

function onBack(event) {
    previousPage(contentWindow);
}

// utility functions
function getUrlFromTag(title) {
    return pagePrefix + title + pagePostfix;
}


// helper functions that actually do stuff
function savePageInfo(win) {
    
}


function nextPage(win) {
    var nextPageTag = wizardMap[currentPageTag].next;
    if (nextPageTag)
        win.location=getUrlFromTag(nextPageTag);
}

function previousPage(win) {
    previousPageTag = wizardMap[currentPageTag].previous;
    if (previousPageTag)
        win.location=getUrlFromTag(previousPageTag)
}

function initializePage(win) {
    
}
