/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

var pickerBundle = srGetStrBundle("chrome://messenger/locale/messenger.properties");

// call this from dialog onload() to set the menu item to the correct value
function MsgFolderPickerOnLoad(pickerID)
{
	//dump("in MsgFolderPickerOnLoad()\n");
	var uri = null;
	try { 
		uri = window.arguments[0].preselectedURI;
	}
	catch (ex) {
		uri = null;
	}

	if (uri) {
		//dump("on loading, set titled button to " + uri + "\n");

		// verify that the value we are attempting to
		// pre-flight the menu with is valid for this
		// picker type
		var msgfolder = GetMsgFolderFromUri(uri);
        	if (!msgfolder) return; 
		
		var verifyFunction = null;

		switch (pickerID) {
			case "msgNewFolderPicker":
				verifyFunction = msgfolder.canCreateSubfolders;
				break;
			case "msgRenameFolderPicker":
				verifyFunction = msgfolder.canRename;
				break;
			default:
				verifyFunction = msgfolder.canFileMessages;
				break;
		}

		if (verifyFunction) {
			SetFolderPicker(uri,pickerID);
		}
	}
}

function PickedMsgFolder(selection,pickerID)
{
	var selectedUri = selection.getAttribute('id');
	SetFolderPicker(selectedUri,pickerID);
}     

function SetFolderPicker(uri,pickerID)
{
	var picker = document.getElementById(pickerID);
	var msgfolder = GetMsgFolderFromUri(uri);

	if (!msgfolder) return;

	var selectedValue = null;
	var serverName;

	if (msgfolder.isServer)
		selectedValue = msgfolder.name;
	else {
		if (msgfolder.server)
			serverName = msgfolder.server.prettyName;
		else {
			dump("Cant' find server for " + uri + "\n");
			serverName = "???";
		}

		selectedValue = pickerBundle.GetStringFromName("verboseFolderFormat")
		                            .replace(/%folderName%/, msgfolder.name)
		                            .replace(/%serverName%/, serverName);
	}

	picker.setAttribute("value",selectedValue);
	picker.setAttribute("uri",uri);
}
