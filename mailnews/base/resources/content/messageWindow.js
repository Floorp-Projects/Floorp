/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

/* This is where functions related to the standalone message window are kept */

/* globals for a particular window */

var compositeDataSourceProgID        = datasourceProgIDPrefix + "composite-datasource";

var gCompositeDataSource;
var gCurrentMessageUri;
var gCurrentFolderUri;

function OnLoadMessageWindow()
{
	CreateMailWindowGlobals();
	CreateMessageWindowGlobals();

	InitMsgWindow();

	messenger.SetWindow(window, msgWindow);
	InitializeDataSources();
	// FIX ME - later we will be able to use onload from the overlay
	OnLoadMsgHeaderPane();

	if(window.arguments && window.arguments.length == 2)
	{
		if(window.arguments[0])
		{
			gCurrentMessageUri = window.arguments[0];
		}
		else
		{
			gCurrentMessageUri = null;
		}

		if(window.arguments[1])
		{
			gCurrentFolderUri = window.arguments[1];
		}
		else
		{
			gCurrentFolderUri = null;
		}
	}	

  setTimeout("OpenURL(gCurrentMessageUri);", 0);

}

function OnUnloadMessageWindow()
{
	messenger.SetWindow(null, null);
}

function CreateMessageWindowGlobals()
{
	gCompositeDataSource = Components.classes[compositeDataSourceProgID].createInstance();
	gCompositeDataSource = gCompositeDataSource.QueryInterface(Components.interfaces.nsIRDFCompositeDataSource);

}

function InitializeDataSources()
{
	AddDataSources();
	//Now add datasources to composite datasource
	gCompositeDataSource.AddDataSource(accountManagerDataSource);
    gCompositeDataSource.AddDataSource(folderDataSource);
	gCompositeDataSource.AddDataSource(messageDataSource);
}

function GetSelectedMsgFolders()
{
	var folderArray = new Array(1);
	var msgFolder = GetLoadedMsgFolder();
	if(msgFolder)
	{
		folderArray[0] = msgFolder;	
	}
	return folderArray;
}

function GetSelectedMessages()
{
	var messageArray = new Array(1);
	var message = GetLoadedMessage();
	if(message)
	{
		messageArray[0] = message;	
	}
	return messageArray;
}

function GetLoadedMsgFolder()
{
	var folderResource = RDF.GetResource(gCurrentFolderUri);
	if(folderResource)
	{
		var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
		return msgFolder;
	}
	return null;
}

function GetLoadedMessage()
{
	var messageResource = RDF.GetResource(gCurrentMessageUri);
	if(messageResource)
	{
		var message = messageResource.QueryInterface(Components.interfaces.nsIMessage);
		return message;
	}
	return null;

}

function GetCompositeDataSource(command)
{
	return gCompositeDataSource;	
}
