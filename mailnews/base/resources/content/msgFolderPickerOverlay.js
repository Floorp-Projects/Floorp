// call this from dialog onload() to set the menu item to the correct value
function MsgFolderPickerOnLoad(pickerID)
{
	dump("in MsgFolderPickerOnLoad()\n");
	var uri = null;
	try { 
		uri = window.arguments[0].preselectedURI;
	}
	catch (ex) {
		uri = null;
	}

	if (uri) {
		dump("on loading, set titled button to " + uri + "\n");
		SetTitleButton(uri,pickerID);
	}
}

function PickedMsgFolder(selection,pickerID)
{
	var selectedUri = selection.getAttribute('id');
	SetTitleButton(selectedUri,pickerID);
}     

function SetTitleButton(uri,pickerID)
{
	var picker = document.getElementById(pickerID);
	var msgfolder = GetMsgFolderFromUri(uri);

	var selectedValue = null;

	if (msgfolder.isServer)
		selectedValue = msgfolder.name;
	else
		selectedValue = msgfolder.name + " on " + msgfolder.server.prettyName; 

	picker.setAttribute("value",selectedValue);
	picker.setAttribute("uri",uri);
}
