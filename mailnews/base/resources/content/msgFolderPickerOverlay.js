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

		if (pickerID == "msgSubscribeFolderPicker") {
			verifyFunction = msgfolder.canSubscribe;
		}
		else if (pickerID == "msgNewFolderPicker") {
			verifyFunction = msgfolder.canCreateSubfolders;
		}
		else if (pickerID == "msgRenameFolderPicker") {
			verifyFunction = msgfolder.canRename;
		}
		else if ((pickerID == "msgFccFolderPicker") || (pickerID == "msgDraftsFolderPicker") || (pickerID == "msgStationeryFolderPicker") || (pickerID == "msgJunkMailFolderPicker")) {
			verifyFunction = msgfolder.canFileMessages;
		}
		else {
			dump("this should never happen\n");
			return;
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

	if (msgfolder.isServer)
		selectedValue = msgfolder.name;
	else
		selectedValue = msgfolder.name + " on " + msgfolder.server.prettyName; 

	picker.setAttribute("value",selectedValue);
	picker.setAttribute("uri",uri);
}
