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
var gFccRadioElemChoice, gDraftsRadioElemChoice, gTmplRadioElemChoice;
var gFccRadioElemChoiceLocked, gDraftsRadioElemChoiceLocked, gTmplRadioElemChoiceLocked;
var gDefaultPickerMode = "1";
var gMessengerBundle;

var gFccFolderWithDelim, gDraftsFolderWithDelim, gTemplatesFolderWithDelim;

// Picker IDs
var fccAccountPickerId = "msgFccAccountPicker";
var fccFolderPickerId = "msgFccFolderPicker";
var draftsAccountPickerId = "msgDraftsAccountPicker";
var draftsFolderPickerId = "msgDraftsFolderPicker";
var tmplAccountPickerId = "msgStationeryAccountPicker";
var tmplFolderPickerId = "msgStationeryFolderPicker";

/* 
 * Set the global radio element choices and initialize folder/account pickers. 
 * Also, initialize other UI elements (bcc self, fcc picker controller checkboxes). 
 */
function onInit() {

    SetGlobalRadioElemChoices();
                     
    SetFolderDisplay(gFccRadioElemChoice, gFccRadioElemChoiceLocked, 
                     "fcc", 
                     fccAccountPickerId, 
                     "identity.fccFolder", 
                     fccFolderPickerId);

    SetFolderDisplay(gDraftsRadioElemChoice, gDraftsRadioElemChoiceLocked, 
                     "draft", 
                     draftsAccountPickerId, 
                     "identity.draftFolder", 
                     draftsFolderPickerId);

    SetFolderDisplay(gTmplRadioElemChoice, gTmplRadioElemChoiceLocked, 
                     "tmpl", 
                     tmplAccountPickerId, 
                     "identity.stationeryFolder", 
                     tmplFolderPickerId);
    initBccSelf();
    setupFccItems();
    gMessengerBundle = document.getElementById("bundle_messenger");
    SetSpecialFolderNamesWithDelims();
}

// Initialize the picker mode choices (account/folder picker) into global vars
function SetGlobalRadioElemChoices()
{
    var pickerModeElement = document.getElementById("identity.fccFolderPickerMode");
    gFccRadioElemChoice = pickerModeElement.getAttribute("value");
    gFccRadioElemChoiceLocked = pickerModeElement.getAttribute("disabled");
    if (!gFccRadioElemChoice) gFccRadioElemChoice = gDefaultPickerMode;

    pickerModeElement = document.getElementById("identity.draftsFolderPickerMode");
    gDraftsRadioElemChoice = pickerModeElement.getAttribute("value");
    gDraftsRadioElemChoiceLocked = pickerModeElement.getAttribute("disabled");
    if (!gDraftsRadioElemChoice) gDraftsRadioElemChoice = gDefaultPickerMode;

    pickerModeElement = document.getElementById("identity.tmplFolderPickerMode");
    gTmplRadioElemChoice = pickerModeElement.getAttribute("value");
    gTmplRadioElemChoiceLocked = pickerModeElement.getAttribute("disabled");
    if (!gTmplRadioElemChoice) gTmplRadioElemChoice = gDefaultPickerMode;
}

// Get Current Server ID selected in the account tree
function GetCurrentServerId()
{
    var tree = window.parent.accounttree;
    var result = getServerIdAndPageIdFromTree(tree);
    return result.serverId;
}

/* 
 * Set Account and Folder elements based on the values read from 
 * preferences file. Default picker mode, if none specified at this stage, is
 * set to 1 i.e., Other picker displaying the folder value read from the 
 * preferences file.
 */
function SetFolderDisplay(pickerMode, disableMode,
                          radioElemPrefix, 
                          accountPickerId, 
                          folderPickedField, 
                          folderPickerId)
{
    if (!pickerMode)
        pickerMode = gDefaultPickerMode;

    var selectAccountRadioId = radioElemPrefix + "_selectAccount";
    var selectAccountRadioElem = document.getElementById(selectAccountRadioId);
    var selectFolderRadioId = radioElemPrefix + "_selectFolder";
    var selectFolderRadioElem = document.getElementById(selectFolderRadioId);

    switch (pickerMode) 
    {
        case "0" :
            selectAccountRadioElem.setAttribute("checked", "true");
            selectFolderRadioElem.setAttribute("checked", "false");
            if (disableMode) {
              selectAccountRadioElem.setAttribute("disabled","true");
              selectFolderRadioElem.setAttribute("disabled","true");
            } else {
              selectAccountRadioElem.removeAttribute("disabled");
              selectFolderRadioElem.removeAttribute("disabled");
            }

            var folderPickedElement = document.getElementById(folderPickedField);
            var uri = folderPickedElement.getAttribute("value");
            var msgFolder = GetMsgFolderFromUri(uri);
            SetFolderPicker(msgFolder.server.serverURI, accountPickerId);
            SetPickerEnabling(accountPickerId, folderPickerId);
            break;

        case "1"  :
            selectFolderRadioElem.setAttribute("checked", "true");
            selectAccountRadioElem.setAttribute("checked", "false");

            if (disableMode) {
              selectAccountRadioElem.setAttribute("disabled","true");
              selectFolderRadioElem.setAttribute("disabled","true");
            } else {
              selectAccountRadioElem.removeAttribute("disabled");
              selectFolderRadioElem.removeAttribute("disabled");
            }
		    	
            InitFolderDisplay(folderPickedField, folderPickerId);
            SetPickerEnabling(folderPickerId, accountPickerId);
            break;
        default :
            dump("Error in setting initial folder display on pickers\n");
            break;
    }
}

// Initialize the folder display based on prefs values
function InitFolderDisplay(fieldname, pickerId) {
    var formElement = document.getElementById(fieldname);
    var uri = formElement.getAttribute("value");
    SetFolderPicker(uri,pickerId);
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
        
        divtag.setAttribute("label", newstr);
    }
}

// Capture any menulist changes
function noteSelectionChange(radioItemId)
{
    var checkedElem = document.getElementById(radioItemId);
    var modeValue  = checkedElem.getAttribute("value");
    var radioGroup = checkedElem.getAttribute("group");
    switch (radioGroup)
    {
        case "doFcc" :
            gFccRadioElemChoice = modeValue;
            break;
    
        case "messageDrafts" :
            gDraftsRadioElemChoice = modeValue;
            break;

        case "messageTemplates" :
            gTmplRadioElemChoice = modeValue;
            break;

        default :
            dump("Error capturing menulist changes.\n");
            break;
    }
}

// Need to append special folders when account picker is selected.
// Create a set of global special folder vars to be suffixed to the
// server URI of the selected account.
function SetSpecialFolderNamesWithDelims()
{
    var folderDelim = "/";
    var sentFolderName =  gMessengerBundle.getString('sentFolderName');
    var draftsFolderName =  gMessengerBundle.getString('draftsFolderName');
    var templatesFolderName =  gMessengerBundle.getString('templatesFolderName');
	
    gFccFolderWithDelim = folderDelim + sentFolderName;
    gDraftsFolderWithDelim = folderDelim + draftsFolderName;
    gTemplatesFolderWithDelim = folderDelim + templatesFolderName;
}

// Save all changes on this page
function onSave()
{
    SaveFolderSettings( gFccRadioElemChoice, 
                        "doFcc",
                        gFccFolderWithDelim, 
                        fccAccountPickerId, 
                        fccFolderPickerId,
                        "identity.fccFolder",
                        "identity.fccFolderPickerMode" );

    SaveFolderSettings( gDraftsRadioElemChoice, 
                        "messageDrafts",
                        gDraftsFolderWithDelim, 
                        draftsAccountPickerId, 
                        draftsFolderPickerId,
                        "identity.draftFolder",
                        "identity.draftsFolderPickerMode" );

    SaveFolderSettings( gTmplRadioElemChoice,
                        "messageTemplates",
                        gTemplatesFolderWithDelim, 
                        tmplAccountPickerId, 
                        tmplFolderPickerId,
                        "identity.stationeryFolder",
                        "identity.tmplFolderPickerMode" );
}

// Save folder settings and radio element choices
function SaveFolderSettings(radioElemChoice, 
                            radioGroupId,
                            folderSuffix,
                            accountPickerId,
                            folderPickerId,
                            folderElementId,
                            folderPickerModeId)
{
    switch (radioElemChoice) 
    {
        case "0" :
            var picker = document.getElementById(accountPickerId);
            var uri = picker.getAttribute("uri");
            if (uri) {
                // Create  Folder URI
                uri = uri + folderSuffix;

                var formElement = document.getElementById(folderElementId);
                formElement.setAttribute("value",uri);
            }
            break;

        case "1" : 
            var picker = document.getElementById(folderPickerId);
            var uri = picker.getAttribute("uri");
            if (uri) {
                SaveUriFromPicker(folderElementId, folderPickerId);
            }
            break;

        default :
            dump ("Error saving folder preferences.\n");
            return;
    }

    var formElement = document.getElementById(folderPickerModeId);
    formElement.setAttribute("value", radioElemChoice);
}

// Get the URI from the picker and save the value into the corresponding pref
function SaveUriFromPicker(fieldName, pickerId)
{
    var picker = document.getElementById(pickerId);
    var uri = picker.getAttribute("uri");
    
    var formElement = document.getElementById(fieldName);
    formElement.setAttribute("value",uri);
}

// Check the Fcc Self item and setup associated picker state 
function setupFccItems()
{ 
    var broadcaster = document.getElementById("broadcaster_doFcc");

    var checked = document.getElementById("identity.doFcc").checked;
    if (checked) {
        broadcaster.removeAttribute("disabled");
        SetupFccPickerState(gFccRadioElemChoice,
                            fccAccountPickerId,
                            fccFolderPickerId);
	}
    else
        broadcaster.setAttribute("disabled", "true");
}

// Set up picker settings for Sent Folder 
function SetupFccPickerState(pickerMode, accountPickerId, folderPickerId)
{
    switch (pickerMode) {
        case "0" :
            if (!gFccRadioElemChoiceLocked)
              SetPickerEnabling(accountPickerId, folderPickerId);
            SetRadioButtons("fcc_selectAccount", "fcc_selectFolder");
            break;
	
        case "1" :
            if (!gFccRadioElemChoiceLocked)
              SetPickerEnabling(folderPickerId, accountPickerId);
            SetRadioButtons("fcc_selectFolder", "fcc_selectAccount");
            break;

        default :
            dump("Error in setting Fcc elements.\n");
            break;
    }
}

// Enable and disable pickers based on the radio element clicked
function SetPickerEnabling(enablePickerId, disablePickerId)
{
    var activePicker = document.getElementById(enablePickerId);
    activePicker.removeAttribute("disabled");

    var inactivePicker = document.getElementById(disablePickerId);
    inactivePicker.setAttribute("disabled", "true");
}

// Set radio element choices and picker states
function setPickersState(enablePickerId, disablePickerId, event)
{
    SetPickerEnabling(enablePickerId, disablePickerId);

    var serverId = GetCurrentServerId();
    var selectedElementUri;
    var radioElemValue = event.target.value;

    var account = parent.getAccountFromServerId(serverId);
    if (!account) return;

    var server = account.incomingServer;

    // if special folders are not to be made on the server, 
    // then Local Folders is the home for it's special folders
    if (!server.defaultCopiesAndFoldersPrefsToServer) {
        selectedElementUri = parent.accountManager.localFoldersServer.serverURI;
    }        
    else
        selectedElementUri = serverId;
    
    switch (event.target.id) {
        case "fcc_selectAccount" :
            gFccRadioElemChoice = radioElemValue;   
            break;
        case "draft_selectAccount" :
            gDraftsRadioElemChoice = radioElemValue;   
            break;
        case "tmpl_selectAccount" :
            gTmplRadioElemChoice = radioElemValue;   
            break;
        case "fcc_selectFolder" :
            gFccRadioElemChoice = radioElemValue;   
            selectedElementUri += gFccFolderWithDelim;
            break;
        case "draft_selectFolder" :
            gDraftsRadioElemChoice = radioElemValue;   
            selectedElementUri += gDraftsFolderWithDelim;
            break;
        case "tmpl_selectFolder" :
            gTmplRadioElemChoice = radioElemValue;   
            selectedElementUri += gTemplatesFolderWithDelim;
            break;
        default :
            dump("Error in setting picker state.\n");
            return;
    }
    
    SetFolderPicker(selectedElementUri, enablePickerId);
}

// This routine is to restore the correct radio element 
// state when the fcc self checkbox broadcasts the change
function SetRadioButtons(selectPickerId, unselectPickerId)
{
    var activeRadioElem = document.getElementById(selectPickerId);
    activeRadioElem.setAttribute("checked", "true");

    var inactiveRadioElem = document.getElementById(unselectPickerId);
    inactiveRadioElem.removeAttribute("checked");
}
