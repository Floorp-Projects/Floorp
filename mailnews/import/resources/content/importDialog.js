/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

var importType = 0;
var bundle = 0;
var importService = 0;
var successStr = null;
var errorStr = null;
var progressInfo = null;
var selectedModuleName = null;



function GetBundleString( strId)
{
	try {
		return( top.bundle.GetStringFromName( strId));
	} catch( ex) {
	}
	
	return( "String Bundle Bad");
}

function GetFormattedBundleString( strId, formatStr)
{
	try {
		return( top.bundle.formatStringFromName( strId, [ formatStr ], 1));
	} catch( ex) {
	}
	return( "String Bundle Bad");
}

function OnLoadImportDialog()
{
	top.bundle = srGetStrBundle("chrome://messenger/locale/importMsgs.properties");
	top.importService = Components.classes["component://mozilla/import/import-service"].getService();
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
	if (window.arguments && window.arguments[0] && window.arguments.importType)
	{
		// keep parameters in global for later
		top.importType = window.arguments[0].importType;
		top.progressInfo.importType = top.importType;
	}
	else
	{
		top.importType = "addressbook";
		top.progressInfo.importType = "addressbook";
	}
	
	SetUpImportType();	
}


function SetUpImportType()
{
	// set dialog title
	switch ( top.importType )
	{
		case "mail":
			document.getElementById( "listLabel").setAttribute('value', GetBundleString( 'ImportMailListLabel'));
			document.getElementById( "mailRadio").checked = true;
			document.getElementById( "addressbookRadio").checked = false;
			document.getElementById( "settingsRadio").checked = false;
			break;
		case "addressbook":
			// top.window.title = top.bundle.GetStringFromName('ImportAddressBooksDialogTitle');
			document.getElementById( "listLabel").setAttribute('value', GetBundleString('ImportAddressBooksListLabel'));
			//SetDivText('listLabel', GetBundleString('ImportAddressBooksListLabel'));
			document.getElementById( "addressbookRadio").checked = true;
			document.getElementById( "mailRadio").checked = false;
			document.getElementById( "settingsRadio").checked = false;
			break;
		case "settings":
			// top.window.title = top.bundle.GetStringFromName('ImportSettingsDialogTitle');
			document.getElementById( "listLabel").setAttribute('value', GetBundleString('ImportSettingsListLabel'));
			//SetDivText('listLabel', GetBundleString('ImportSettingsListLabel'));
			document.getElementById( "settingsRadio").checked = true;
			document.getElementById( "addressbookRadio").checked = false;
			document.getElementById( "mailRadio").checked = false;
			break;
	}
	
	ListModules();
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
		else if ( div.childNodes.length == 1 ) {			
			div.childNodes[0].nodeValue = text;
		}
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
		top.selectedModuleName = name;
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
							var meterText = GetFormattedBundleString( 'MailProgressMeterText', name);
							// show the progress window...
							top.window.openDialog(
								"chrome://messenger/content/importProgress.xul",
								"",
								"chrome,modal,titlebar",
								{windowTitle: GetBundleString( 'MailProgressTitle'),
								 progressTitle: meterText,
								 progressStatus: "",
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
							var meterText = GetFormattedBundleString( 'AddrProgressMeterText', name);
							var titleText = GetBundleString( 'AddrProgressTitle');
							// show the progress window...
							top.window.openDialog(
								"chrome://messenger/content/importProgress.xul",
								"",
								"chrome,modal,titlebar",
								{windowTitle: titleText,
								 progressTitle: meterText,
								 progressStatus: "",
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
							alert( GetBundleString( 'ImportSettingsFailed'));
						}
						// the user canceled the operation, shoud we dismiss
						// this dialog or not?
						return false;
					}
					else
					{
						// Alert to show success
						alert( GetFormattedBundleString( 'ImportSettingsSuccess', name));
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
	
	var body = document.getElementById( "bucketBody");
	var max = body.childNodes.length - 1;
	while (max >= 0) {
		body.removeChild( body.childNodes[max]);
		max--;
	}
		
	var count = top.importService.GetModuleCount( top.importType);	
	for (var i = 0; i < count; i++) {
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

	/* dump( "*** ContinueImport\n"); */

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
				if (pcnt < 5)
					pcnt = 5;
				info.progressWindow.SetProgress( pcnt);
				if (isMail == true) {
					var mailName = info.importInterface.GetData( "currentMailbox");
					if (mailName != null) {
						mailName = mailName.QueryInterface( Components.interfaces.nsISupportsWString);
						if (mailName != null)
							info.progressWindow.SetStatusText( mailName.data);
					}
				}
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
	var str = null;
	if (good == true) {
		if ((top.selectedModuleName != null) && (top.selectedModuleName.length > 0))
			str = GetFormattedBundleString( 'ImportMailSuccess', top.selectedModuleName);
		else
			str = GetFormattedBundleString( 'ImportMailSuccess', "");

		str += "\n";
		if ((top.successStr.data != null) && (top.successStr.data.length > 0))
			str += "\n" + "\n" + top.successStr.data;
	}
	else {
		if ((top.errorStr.data != null) && (top.errorStr.data.length > 0)) {
			if ((top.selectedModuleName != null) && (top.selectedModuleName.length > 0))
				str = GetFormattedBundleString( 'ImportMailFailed', top.selectedModuleName);
			else
				str = GetFormattedBundleString( 'ImportMailFailed', "");
			str += "\n" + top.errorStr.data;
		}
	}
	
	if (str != null)
		alert( str);
}


function ShowAddressComplete( good)
{
	var str = null;
	if (good == true) {
		if ((top.selectedModuleName != null) && (top.selectedModuleName.length > 0))
			str = GetFormattedBundleString( 'ImportAddressSuccess', top.selectedModuleName);
		else
			str = GetFormattedBundleString( 'ImportAddressSuccess', "");
		str += "\n";
		str += "\n" + top.successStr.data;
	}
	else {
		if ((top.errorStr.data != null) && (top.errorStr.data.length > 0)) {
			if ((top.selectedModuleName != null) && (top.selectedModuleName.length > 0))
				str = GetFormattedBundleString( 'ImportAddressFailed', top.selectedModuleName);
			else
				str = GetFormattedBundleString( 'ImportAddressFailed', "");
			str += "\n" + top.errorStr.data;
		}
	}
	
	if (str != null)
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
		error.value = GetBundleString( 'ImportSettingsBadModule');
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
					// filePicker.create( window.top, GetBundleString( 'ImportSelectSettings'), filePicker.modeLoad);
					try {
						filePicker.chooseInputFile( GetBundleString( 'ImportSelectSettings'), filePicker.eAllFiles, null, null);
						setIntf.SetLocation( filePicker.QueryInterface( Components.interfaces.nsIFileSpec));
					} catch( ex) {
						error.value = null;
						return( false);
					}					
				}
				else {
					error.value = GetBundleString( 'ImportSettingsNotFound');
					return( false);
				}
			}
			else {
				error.value = GetBundleString( 'ImportSettingsNotFound');
				return( false);
			}
		}
		else {
			error.value = GetBundleString( 'ImportSettingsNotFound');
			return( false);
		}
	}

	// interesting, we need to return the account that new
	// mail should be imported into?
	// that's really only useful for "Upgrade"
	var result = setIntf.Import( newAccount);
	if (result == false) {
		error.value = GetBundleString( 'ImportSettingsFailed');
	}
	return( result);
}

function CreateNewFileSpec( inFile)
{
	var file = Components.classes["component://netscape/filespec"].createInstance();
	if (file != null) {
		file = file.QueryInterface( Components.interfaces.nsIFileSpec);
		if (file != null) {
			file.fromFileSpec( inFile);
		}
	}

	return( file);
}

function ImportMail( module, success, error) {
	if (top.progressInfo.importInterface || top.progressInfo.intervalState) {
		error.data = GetBundleString( 'ImportAlreadyInProgress');
		return( false);
	}
	
	top.progressInfo.importSuccess = false;

	var mailInterface = module.GetImportInterface( "mail");
	if (mailInterface != null)
		mailInterface = mailInterface.QueryInterface( Components.interfaces.nsIImportGeneric);
	if (mailInterface == null) {
		error.data = GetBundleString( 'ImportMailBadModule');
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
						filePicker.chooseDirectory( GetBundleString( 'ImportSelectMailDir'));
						mailInterface.SetData( "mailLocation", CreateNewFileSpec( filePicker.QueryInterface( Components.interfaces.nsIFileSpec)));
					} catch( ex) {
						// don't show an error when we return!
						return( false);
					}					
				}
				else {
					error.data = GetBundleString( 'ImportMailNotFound');
					return( false);
				}
			}
			else {
				error.data = GetBundleString( 'ImportMailNotFound');
				return( false);
			}
		}
		else {
			error.data = GetBundleString( 'ImportMailNotFound');
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
		error.data = GetBundleString( 'ImportAlreadyInProgress');
		return( false);
	}

	top.progressInfo.importSuccess = false;

	var addInterface = module.GetImportInterface( "addressbook");
	if (addInterface != null)
		addInterface = addInterface.QueryInterface( Components.interfaces.nsIImportGeneric);
	if (addInterface == null) {
		error.data = GetBundleString( 'ImportAddressBadModule');
		return( false);
	}
	
	
	var loc = addInterface.GetStatus( "autoFind");
	if (loc == false) {
		loc = addInterface.GetData( "addressLocation");
		if (loc != null) {
			loc = loc.QueryInterface( Components.interfaces.nsIFileSpec);
			if (loc != null) {
				if (!loc.exists)
					loc = null;
			}
		}
	}
	
	if (loc == null) {
		// Couldn't find the address book, see if we can
		// as the user for the location or not?
		if (addInterface.GetStatus( "canUserSetLocation") == 0) {
			// an autofind address book that could not be found!
			error.data = GetBundleString( 'ImportAddressNotFound');
			return( false);
		}

		var filePicker = Components.classes["component://netscape/filespecwithui"].createInstance();
		if (filePicker != null) {
			filePicker = filePicker.QueryInterface( Components.interfaces.nsIFileSpecWithUI);
			if (filePicker == null) {
				error.data = GetBundleString( 'ImportAddressNotFound');
				return( false);
			}
		}
		else {
			error.data = GetBundleString( 'ImportAddressNotFound');
			return( false);
		}

		// The address book location was not found.
		// Determine if we need to ask for a directory
		// or a single file.
		var file = null;
		if (addInterface.GetStatus( "supportsMultiple") != 0) {
			// ask for dir
			try {
				filePicker.chooseDirectory( GetBundleString( 'ImportSelectAddrDir'));
				file = filePicker.QueryInterface( Components.interfaces.nsIFileSpec);
			} catch( ex) {
				file = null;
			}
		}
		else {
			// ask for file
			try {
				filePicker.chooseInputFile( GetBundleString( 'ImportSelectAddrFile'), filePicker.eAllFiles, null, null);
				file = filePicker.QueryInterface( Components.interfaces.nsIFileSpec);
			} catch( ex) {
				file = null;
			}
		}

		if (file == null) {
			return( false);
		}
		
		file = CreateNewFileSpec( file);

		addInterface.SetData( "addressLocation", file);
	}
	
	var map = addInterface.GetData( "fieldMap");
	if (map != null) {
		map = map.QueryInterface( Components.interfaces.nsIImportFieldMap);
		if (map != null) {
			var result = new Object();
			result.ok = false;
			top.window.openDialog(
				"chrome://messenger/content/fieldMapImport.xul",
				"",
				"chrome,modal,titlebar",
				{fieldMap: map,
				 addInterface: addInterface,
				 result: result});
		}
		if (result.ok == false)
			return( false);
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

function SwitchType( newType)
{
	top.importType = newType;
	top.progressInfo.importType = newType;
	
	SetUpImportType();	
	
	SetDivText('description', "");
}

function OnMailType()
{
	SwitchType( "mail");
}

function OnSettingsType()
{
	SwitchType( "settings");
}

function OnAddressType()
{
	SwitchType( "addressbook");
}

