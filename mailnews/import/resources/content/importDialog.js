var importType = 0;
var bundle = 0;
var importService = 0;
var successStr = null;
var errorStr = null;
var progressInfo = null;

function OnLoadImportDialog()
{
	top.bundle = srGetStrBundle("chrome://messenger/locale/importMsgs.properties");
	top.importService = Components.classes["component://mozilla/import/import-service"].createInstance();
	top.importService = top.importService.QueryInterface(Components.interfaces.nsIImportService);
	
	top.progressInfo = new Object();
	top.progressInfo.progressWindow = null;
	top.progressInfo.importInterface = null;
	top.progressInfo.mainWindow = top.window;
	top.progressInfo.intervalState = 0;
	top.progressInfo.importSuccess = false;
	top.progressInfo.importType = null;

	doSetOKCancel(ImportDialogOKButton, 0);

	// look in arguments[0] for parameters
	if (window.arguments && window.arguments[0])
	{
		// keep parameters in global for later
		if ( window.arguments[0].importType )
			top.importType = window.arguments[0].importType;
		top.progressInfo.importType = top.importType;
	
		// set dialog title
		switch ( top.importType )
		{
			case "mail":
				top.window.title = top.bundle.GetStringFromName('ImportMailDialogTitle');
				SetDivText('listLabel', top.bundle.GetStringFromName('ImportMailListLabel'));
				break;
			case "addressbook":
				top.window.title = top.bundle.GetStringFromName('ImportAddressBooksDialogTitle');
				SetDivText('listLabel', top.bundle.GetStringFromName('ImportAddressBooksListLabel'));
				break;
			case "settings":
				top.window.title = top.bundle.GetStringFromName('ImportSettingsDialogTitle');
				SetDivText('listLabel', top.bundle.GetStringFromName('ImportSettingsListLabel'));
				break;
		}
		ListModules();
		
		// add module names to tree
	}
}

function SetDivText(id, text)
{
	var div = document.getElementById(id);
	
	if ( div )
	{
		if ( div.childNodes.length == 0 )
		{
			var textNode = document.createTextNode(text);
			div.appendChild(textNode);                   			
		}
		else if ( div.childNodes.length == 1 )
			div.childNodes[0].nodeValue = text;
	}
}


function ImportDialogOKButton()
{
	var tree = document.getElementById('moduleList');
	if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
	{
		var index = tree.selectedItems[0].getAttribute('list-index');
		var module = top.importService.GetModule( top.importType, index);
		var name = top.importService.GetModuleName( top.importType, index);
		if (module != null) 
		{
			switch( top.importType )
			{
				case "mail":
					top.successStr = Components.classes["component://netscape/supports-wstring"].createInstance();
					if (top.successStr != null) {
						top.successStr = top.successStr.QueryInterface( Components.interfaces.nsISupportsWString);
					}
					top.errorStr = Components.classes["component://netscape/supports-wstring"].createInstance();
					if (top.errorStr != null)
						top.errorStr = top.errorStr.QueryInterface( Components.interfaces.nsISupportsWString);
					
					if (ImportMail( module, top.successStr, top.errorStr) == true) 
					{
						// We think it was a success, either, we need to 
						// wait for the import to finish
						// or we are done!
						if (top.progressInfo.importInterface == null) {
							ShowMailComplete( true);
							return( true);
						}
						else {
							// show the progress window...
							top.window.openDialog(
								"chrome://messenger/content/importProgress.xul",
								"",
								"chrome,modal",
								{windowTitle:"My Progress Window Title",
								 progressTitle: "Importing some crap",
								 progressStatus: "Now importing specific crap",
								 progressInfo: top.progressInfo});
							
							dump( "*** Returned from progress window\n");

							return( true);
						}
					}
					else 
					{
						ShowMailComplete( false);
						return( false);
					}
					break;
					
				case "addressbook":
					top.successStr = Components.classes["component://netscape/supports-wstring"].createInstance();
					if (top.successStr != null) {
						top.successStr = top.successStr.QueryInterface( Components.interfaces.nsISupportsWString);
					}
					top.errorStr = Components.classes["component://netscape/supports-wstring"].createInstance();
					if (top.errorStr != null)
						top.errorStr = top.errorStr.QueryInterface( Components.interfaces.nsISupportsWString);
					
					if (ImportAddress( module, top.successStr, top.errorStr) == true) 
					{
						// We think it was a success, either, we need to 
						// wait for the import to finish
						// or we are done!
						if (top.progressInfo.importInterface == null) {
							ShowAddressComplete( true);
							return( true);
						}
						else {
							// show the progress window...
							top.window.openDialog(
								"chrome://messenger/content/importProgress.xul",
								"",
								"chrome,modal",
								{windowTitle:"My Address Window Title",
								 progressTitle: "Importing some address",
								 progressStatus: "Now importing specific crap",
								 progressInfo: top.progressInfo});

							return( true);
						}
					}
					else 
					{
						ShowAddressComplete( false);
						return( false);
					}
					break;

				case "settings":
					var error = new Object();
					error.value = null;
					var newAccount = new Object();
					if (!ImportSettings( module, newAccount, error)) 
					{
						if (error.value != null)
						{
							// Show error alert with error
							// information
							alert( top.bundle.GetStringFromName( 'ImportSettingsError') + name + ":\n" + error.value);
						}
						// the user canceled the operation, shoud we dismiss
						// this dialog or not?
						return false;
					}
					else
					{
						// Alert to show success
						alert( top.bundle.GetStringFromName( 'ImportSettingsSuccess') + name);
					}
					break;
			}
		}
	}
	
	return true;
}


function ImportSelectionChanged()
{
	var tree = document.getElementById('moduleList');
	if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
	{
		var index = tree.selectedItems[0].getAttribute('list-index');
		SetDivText('description', top.importService.GetModuleDescription(top.importType, index));
	}
}

function ListModules() {
	if (top.importService == null)
		return;
	var count = top.importService.GetModuleCount( top.importType);
	for (i = 0; i < count; i++) {
		AddModuleToList( top.importService.GetModuleName( top.importType, i), i);
	}
}

function AddModuleToList(moduleName, index)
{
	var body = document.getElementById("bucketBody");
	
	var item = document.createElement('treeitem');
	var row = document.createElement('treerow');
	var cell = document.createElement('treecell');
	cell.setAttribute('value', moduleName);
	item.setAttribute('list-index', index);
	
	row.appendChild(cell);
	item.appendChild(row);
	body.appendChild(item);
}


function ContinueImport( info) {
	var isMail = false;

	dump( "*** ContinueImport\n");

	if (info.importType == 'mail')
		isMail = true;
	else
		isMail = false;

	var clear = true;
	if (info.importInterface) {
		if (!info.importInterface.ContinueImport()) {
			info.importSuccess = false;
			clearInterval( info.intervalState);
			if (info.progressWindow != null) {
				info.progressWindow.close();
				info.progressWindow = null;
			}

			if (isMail == true)
				ShowMailComplete( false);
			else
				ShowAddressComplete( false);
		}
		else if ((pcnt = info.importInterface.GetProgress()) < 100) {
			clear = false;
			if (info.progressWindow != null) {
				info.progressWindow.SetProgress( pcnt);
			}
		}
		else {
			clearInterval( info.intervalState);
			info.importSuccess = true;
			if (info.progressWindow != null) {
				info.progressWindow.close();
				info.progressWindow = null;
			}

			if (isMail == true)
				ShowMailComplete( true);
			else
				ShowAddressComplete( true);
		}
	}
	else {
		dump( "*** ERROR: info.importInterface is null\n");
	}

	if (clear == true) {
		info.intervalState = null;
		info.importInterface = null;
	}
}


function ShowMailComplete( good)
{
	var str;
	if (good == true) {
		str = top.bundle.GetStringFromName( 'ImportMailSuccess');
		str += "\nsuccess: " + top.successStr.data + "\nerror:" + top.errorStr.data;
	}
	else {
		str = top.bundle.GetStringFromName( 'ImportMailFailed');
		str += "\nerror: " + top.errorStr.data;
	}
	
	alert( str);
}


function ShowAddressComplete( good)
{
	var str;
	if (good == true) {
		str = top.bundle.GetStringFromName( 'ImportAddressSuccess');
		str += "\nsuccess: " + top.successStr.data + "\nerror:" + top.errorStr.data;
	}
	else {
		str = top.bundle.GetStringFromName( 'ImportAddressFailed');
		str += "\nerror: " + top.errorStr.data;
	}
	
	alert( str);
}


/*
	Import Settings from a specific module, returns false if it failed
	and true if successful.  A "local mail" account is returned in newAccount.
	This is only useful in upgrading - import the settings first, then
	import mail into the account returned from ImportSettings, then
	import address books.
	An error string is returned as error.value
*/
function ImportSettings( module, newAccount, error) {
	var setIntf = module.GetImportInterface( "settings");
	if (setIntf != null)
		setIntf = setIntf.QueryInterface( Components.interfaces.nsIImportSettings);
	if (setIntf == null) {
		error.value = top.bundle.GetStringFromName( 'ImportSettingsBadModule');
		return( false);
	}
	
	// determine if we can auto find the settings or if we need to ask the user
	var location = new Object();
	var description = new Object();
	var result = setIntf.AutoLocate( description, location);
	if (result == false) {
		// In this case, we couldn't not find the settings
		if (location.value != null) {
			// Settings were not found, however, they are specified
			// in a file, so ask the user for the settings file.
			var filePicker = Components.classes["component://netscape/filespecwithui"].createInstance();
			if (filePicker != null) {
				filePicker = filePicker.QueryInterface( Components.interfaces.nsIFileSpecWithUI);
				if (filePicker != null) {
					// filePicker.create( window.top, "Select settings file", filePicker.modeLoad);
					try {
						filePicker.chooseInputFile( "Select settings file", filePicker.eAllFiles, null, null);
						setIntf.SetLocation( filePicker.QueryInterface( Components.interfaces.nsIFileSpec));
					} catch( ex) {
						error.value = null;
						return( false);
					}					
				}
				else {
					error.value = top.bundle.GetStringFromName( 'ImportSettingsNotFound');
					return( false);
				}
			}
			else {
				error.value = top.bundle.GetStringFromName( 'ImportSettingsNotFound');
				return( false);
			}
		}
		else {
			error.value = top.bundle.GetStringFromName( 'ImportSettingsNotFound');
			return( false);
		}
	}

	// interesting, we need to return the account that new
	// mail should be imported into?
	// that's really only useful for "Upgrade"
	var result = setIntf.Import( newAccount);
	if (result == false) {
		error.value = top.bundle.GetStringFromName( 'ImportSettingsFailed');
	}
	return( result);
}


function ImportMail( module, success, error) {
	if (top.progressInfo.importInterface || top.progressInfo.intervalState) {
		error.data = top.bundle.GetStringFromName( 'ImportAlreadyInProgress');
		return( false);
	}
	
	top.progressInfo.importSuccess = false;

	var mailInterface = module.GetImportInterface( "mail");
	if (mailInterface != null)
		mailInterface = mailInterface.QueryInterface( Components.interfaces.nsIImportGeneric);
	if (mailInterface == null) {
		error.data = top.bundle.GetStringFromName( 'ImportMailBadModule');
		return( false);
	}
	
	var loc = mailInterface.GetData( "mailLocation");
	
	if (loc == null) {
		// No location found, check to see if we can ask the user.
		if (mailInterface.GetStatus( "canUserSetLocation") != 0) {
			var filePicker = Components.classes["component://netscape/filespecwithui"].createInstance();
			if (filePicker != null) {
				filePicker = filePicker.QueryInterface( Components.interfaces.nsIFileSpecWithUI);
				if (filePicker != null) {
					try {
						filePicker.chooseDirectory( "Select mail directory");
						mailInterface.SetLocation( filePicker.QueryInterface( Components.interfaces.nsIFileSpec));
					} catch( ex) {
						// don't show an error when we return!
						return( false);
					}					
				}
				else {
					error.data = top.bundle.GetStringFromName( 'ImportMailNotFound');
					return( false);
				}
			}
			else {
				error.data = top.bundle.GetStringFromName( 'ImportMailNotFound');
				return( false);
			}
		}
		else {
			error.data = top.bundle.GetStringFromName( 'ImportMailNotFound');
			return( false);
		}
	}

	if (mailInterface.WantsProgress()) {
		if (mailInterface.BeginImport( success, error)) {
			top.progressInfo.importInterface = mailInterface;
			// top.intervalState = setInterval( "ContinueImport()", 100);
			return( true);
		}
		else {
			return( false);
		}
	}
	else {
		dump( "*** WantsProgress returned false\n");

		if (mailInterface.BeginImport( success, error)) {
			return( true);
		}
		else {
			return( false);
		}
	}
}


// The address import!  A little more complicated than the mail import
// due to field maps...
function ImportAddress( module, success, error) {
	if (top.progressInfo.importInterface || top.progressInfo.intervalState) {
		error.data = top.bundle.GetStringFromName( 'ImportAlreadyInProgress');
		return( false);
	}

	top.progressInfo.importSuccess = false;

	var addInterface = module.GetImportInterface( "addressbook");
	if (addInterface != null)
		addInterface = addInterface.QueryInterface( Components.interfaces.nsIImportGeneric);
	if (addInterface == null) {
		error.data = top.bundle.GetStringFromName( 'ImportAddressBadModule');
		return( false);
	}
	
	
	var loc = addInterface.GetStatus( "autoFind");
	if (loc == false) {
		loc = addInterface.GetData( "addressLocation");
		if (loc != null) {
			if (!loc.exists)
				loc = null;
		}
	}
	
	if (loc == null) {
		// Couldn't find the address book, see if we can
		// as the user for the location or not?
		if (addInterface.GetStatus( "canUserSetLocation") == 0) {
			// an autofind address book that could not be found!
			error.data = top.bundle.GetStringFromName( 'ImportAddressNotFound');
			return( false);
		}

		var filePicker = Components.classes["component://netscape/filespecwithui"].createInstance();
		if (filePicker != null) {
			filePicker = filePicker.QueryInterface( Components.interfaces.nsIFileSpecWithUI);
			if (filePicker == null) {
				error.data = top.bundle.GetStringFromName( 'ImportAddressNotFound');
				return( false);
			}
		}
		else {
			error.data = top.bundle.GetStringFromName( 'ImportAddressNotFound');
			return( false);
		}

		// The address book location was not found.
		// Determine if we need to ask for a directory
		// or a single file.
		var file = null;
		if (addInterface.GetStatus( "supportsMultiple") != 0) {
			// ask for dir
			try {
				filePicker.chooseDirectory( "Select address book directory");
				file = filePicker.QueryInterface( Components.interfaces.nsIFileSpec);
			} catch( ex) {
				file = null;
			}
		}
		else {
			// ask for file
			try {
				filePicker.chooseInputFile( "Select address book file", filePicker.eAllFiles, null, null);
				file = filePicker.QueryInterface( Components.interfaces.nsIFileSpec);
			} catch( ex) {
				file = null;
			}
		}

		if (file == null) {
			return( false);
		}

		addInterface.SetData( "addressLocation", file);
	}
	
	var map = addInterface.GetData( "fieldMap");
	if (map != null) {
		// FIXME: Show modal UI to handle the field map!
	}

	if (addInterface.WantsProgress()) {
		if (addInterface.BeginImport( success, error)) {
			top.progressInfo.importInterface = addInterface;
			// top.intervalState = setInterval( "ContinueImport()", 100);
			return( true);
		}
		else {
			return( false);
		}
	}
	else {
		if (addInterface.BeginImport( success, error)) {
			return( true);
		}
		else {
			return( false);
		}
	}
}
