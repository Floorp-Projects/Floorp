/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gMessengerBundle;

// call this from dialog onload() to set the menu item to the correct value
function MsgFolderPickerOnLoad(pickerID)
{
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
  if (!gMessengerBundle)
    gMessengerBundle = document.getElementById("bundle_messenger");

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

		selectedValue = gMessengerBundle.getFormattedString("verboseFolderFormat",
															[msgfolder.name,
															serverName]);
	}

	picker.setAttribute("label",selectedValue);
	picker.setAttribute("uri",uri);
}
