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

var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);

function onInit() {
    initFolderDisplay("identity.fccFolder", "fccFolderVerbose");
    initFolderDisplay("identity.draftFolder", "draftFolderVerbose");
    initFolderDisplay("identity.stationaryFolder", "stationaryFolderVerbose");
    initFolderDisplay("identity.junkMailFolder", "junkMailFolderVerbose");
    initBccSelf();
    dump("document is " + document + "\n");
}

function initFolderDisplay(fieldname, divname) {

    var formElement = document.getElementById(fieldname);

    var folder = getFolder(formElement.value);
    
    var verboseName = "";
    if (folder)
        verboseName = folder.prettyName;

    setDivText(divname, verboseName);
}

function initBccSelf() {
    var bccValue = document.getElementById("identity.email").value;
    setDivText("bccemail",bccValue);
}

function setDivText(divid, str) {
    var divtag = document.getElementById(divid);
    dump("setting " + divtag + " to " + str + "\n");
    if (divtag) {
        if (divtag.firstChild)
            divtag.removeChild(divtag.firstChild);
        divtag.appendChild(document.createTextNode(str));
    }
}

function getFolder(uri) {
    if (uri) {
        var res = RDF.GetResource(uri);
        if (res)
            return res.QueryInterface(Components.interfaces.nsIMsgFolder);
    }
    return null;
}

