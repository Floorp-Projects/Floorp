var gMsgFolder;
var preselectedFolderURI = null;

// services used
var RDF;

// corresponds to MSG_FOLDER_FLAG_OFFLINE
const FOLDER_FLAG_OFFLINE = 0x8000000

function folderPropsOKButtonCallback()
{
  if (gMsgFolder)
  {
    if (document.getElementById("selectForDownload").checked)	
      gMsgFolder.setFlag(FOLDER_FLAG_OFFLINE);
    else
      gMsgFolder.clearFlag(FOLDER_FLAG_OFFLINE);

    // set charset attributes
    var folderCharsetList = document.getElementById("folderCharsetList");
    gMsgFolder.charset = folderCharsetList.getAttribute("data");
    gMsgFolder.charsetOverride = document.getElementById("folderCharsetOverride").checked;
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

  if (gMsgFolder) {
    if (gMsgFolder.flags & FOLDER_FLAG_OFFLINE) {
  	  document.getElementById("selectForDownload").checked = true;
    }

    // get charset title (i.e. localized name), needed in order to set value for the menu
    var ccm = Components.classes['@mozilla.org/charset-converter-manager;1'];
    ccm = ccm.getService();
    ccm = ccm.QueryInterface(Components.interfaces.nsICharsetConverterManager2);
    // get a localized string
    var charsetTitle = ccm.GetCharsetTitle(ccm.GetCharsetAtom(gMsgFolder.charset));

    // select the menu item 
    var folderCharsetList = document.getElementById("folderCharsetList");
    folderCharsetList.setAttribute("value", charsetTitle);
    folderCharsetList.setAttribute("data", gMsgFolder.charset);

    // set override checkbox
    document.getElementById("folderCharsetOverride").checked = gMsgFolder.charsetOverride;
  }

  // select the initial tab
  if (window.arguments[0].tabID) {
    // set index for starting panel on the <tabpanel> element
    var folderPropTabPanel = document.getElementById("folderPropTabPanel");
    folderPropTabPanel.setAttribute("index", window.arguments[0].tabIndex);

    var tab = document.getElementById(window.arguments[0].tabID);
    tab.setAttribute("selected", "true");
    tab = document.getElementById("GeneralTab");
    tab.setAttribute("selected", "false");
  }
}
