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

var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

function onInit() {

    initFolderDisplay("identity.fccFolder", "msgFccFolderPicker");
    initFolderDisplay("identity.draftFolder", "msgDraftsFolderPicker");
    initFolderDisplay("identity.stationeryFolder", "msgStationeryFolderPicker");
    initBccSelf();
    //dump("document is " + document + "\n");

}

function initFolderDisplay(fieldname, pickerID) {
    var formElement = document.getElementById(fieldname);
    var uri = formElement.getAttribute("value");
    SetFolderPicker(uri,pickerID);
}

function initBccSelf() {
    var bccValue = document.getElementById("identity.email").getAttribute("value");
    setDivText("identity.bccSelf",bccValue);
}

function setDivText(divid, str) {
    var divtag = document.getElementById(divid);

    var newstr="";
    if (divtag) {
        
        if (divtag.getAttribute("before"))
            newstr += divtag.getAttribute("before");
        
        newstr += str;
        
        if (divtag.getAttribute("after"))
            newstr += divtag.getAttribute("after");
        
        divtag.setAttribute("value", newstr);
    }
}

function onSave()
{
    SaveUriFromPicker("identity.fccFolder", "msgFccFolderPicker");
    SaveUriFromPicker("identity.draftFolder", "msgDraftsFolderPicker");
    SaveUriFromPicker("identity.stationeryFolder", "msgStationeryFolderPicker");
}

function SaveUriFromPicker(fieldName, pickerID)
{
	var picker = document.getElementById(pickerID);
	var uri = picker.getAttribute("uri");
	//dump("uri = " + uri + "\n");
	
	formElement = document.getElementById(fieldName);
	//dump("old value = " + formElement.value + "\n");
	formElement.setAttribute("value",uri);
	//dump("new value = " + formElement.value + "\n");
}
