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

	if (msgfolder.isServer)
		selectedValue = msgfolder.name;
	else {
		if (msgfolder.server) {
			selectedValue = msgfolder.name + " on " + msgfolder.server.prettyName; 
		}
		else {
			selectedValue = msgfolder.name + " on ???";
		}
	}

	picker.setAttribute("value",selectedValue);
	picker.setAttribute("uri",uri);
}
