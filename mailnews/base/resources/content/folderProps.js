var gMsgFolder;
var preselectedFolderURI = null;

// services used
var RDF;


function folderPropsOKButtonCallback()
{
  if (gMsgFolder)
  {
    if (document.getElementById("selectForDownload").checked)	
      gMsgFolder.setFlag(0x8000000);
    else
      gMsgFolder.clearFlag(0x8000000);
  }
  window.close();
}


function folderPropsOnLoad()
{
  dump("folder props loaded"+'\n');
	doSetOKCancel(folderPropsOKButtonCallback);
	moveToAlertPosition();

  RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
	// look in arguments[0] for parameters
	if (window.arguments && window.arguments[0]) {
		if ( window.arguments[0].title ) {
			// dump("title = " + window.arguments[0].title + "\n");
			top.window.title = window.arguments[0].title;
		}
		
		if ( window.arguments[0].okCallback ) {
			top.okCallback = window.arguments[0].okCallback;
		}
	}
	
	// fill in folder name, based on what they selected in the folder pane
	if (window.arguments[0].preselectedURI) {
		try {
			preselectedFolderURI = window.arguments[0].preselectedURI;
		}
		catch (ex) {
		}
	}
	else {
		dump("passed null for preselectedURI, do nothing\n");
	}

	if(window.arguments[0].name)
	{
		var name = document.getElementById("name");
		name.value = window.arguments[0].name;
//		name.setSelectionRange(0,-1);
//		name.focusTextField();

	}
	// this hex value come from nsMsgFolderFlags.h
		var folderResource = RDF.GetResource(preselectedFolderURI);
    
		if(folderResource)
			gMsgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
  if (!gMsgFolder)
    dump("no gMsgFolder preselectfolder uri = "+preselectedFolderURI+'\n');

  if (gMsgFolder && (gMsgFolder.flags & 0x8000000))
  	document.getElementById("selectForDownload").checked = true;

}
