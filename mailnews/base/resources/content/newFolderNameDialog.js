var okCallback = 0;

function newFolderNameOnLoad()
{
	doSetOKCancel(newFolderNameOKButton, 0);

	// look in arguments[0] for parameters
	if (window.arguments && window.arguments[0])
	{
		if ( window.arguments[0].title )
		{
			// dump("title = " + window.arguments[0].title + "\n");
			var title = window.arguments[0].title;
			top.window.title = title;
		}
		
		if ( window.arguments[0].okCallback )
			top.okCallback = window.arguments[0].okCallback;
	}
	
	// pre select the option, based on what they selected in the folder pane
	var selectedParentFolder = document.getElementById('selectedparentfolder');
	// dump("selectedParentFolder = " + selectedParentFolder + "\n");

	try {
        	options = selectedParentFolder.options;
		for (i=1;i<options.length;i++) {
			var uri = options[i].getAttribute('uri');
			// dump(uri + " vs " + window.arguments[0].preselectedURI + "\n");
			if (uri == window.arguments[0].preselectedURI) {
				// dump("preselect: " + uri + " index = " + i + "\n");
				selectedParentFolder.selectedIndex = i;
				break;
			}
		}
        }
	catch (ex) {
		// dump("failed to preflight the select thing.\n");
	}
}

function newFolderNameOKButton()
{
	if ( top.okCallback )
	{
		var name = document.getElementById('name').value;
	
		top.okCallback(name);
	}
	
	return true;
}
