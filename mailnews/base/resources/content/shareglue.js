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

/*
 * code in here is generic, shared utility code across all messenger
 * components. There should be no command or widget specific code here
 */

function NewBrowserWindow() {}
function NewBlankPage() {} 
function TemplatePage() {}
function WizardPage() {}
function PageSetup() {}
function PrintPreview() {}
function Print() {}

function OnLoadMessenger()
{
	messenger.SetWindow(window);
    window.frames["messagepane"].location =
      "http://www.mozilla.org/mailnews/prefs-info.html";
}

function OnUnloadMessenger()
{
	dump("\nOnUnload from XUL\nClean up ...\n");
	messenger.OnUnload();
}

function CloseMessenger() 
{
	dump("\nClose from XUL\nDo something...\n");
	messenger.Close();
}

function Exit()
{
  dump("\nExit from XUL\n");
  messenger.Exit();
}

function CharacterSet(){}

function MessengerSetDefaultCharacterSet(aCharset)
{
    dump(aCharset);dump("\n");
    messenger.SetDocumentCharset(aCharset);
	var folderResource = GetSelectedFolderResource();
	SetFolderCharset(folderResource, aCharset);
    MsgReload();
}

function NavigatorWindow()
{
	var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
	if (!toolkitCore)
	{
		toolkitCore = new ToolkitCore();
		if (toolkitCore)
		{
			toolkitCore.Init("ToolkitCore");
		}
    }

    if (toolkitCore)
	{
      toolkitCore.ShowWindow("chrome://navigator/content/",
                             window);
    }


}
function MessengerWindow() {}
function ComposerWindow() {}
function AIMService() {}
function AddBookmark() {}
function FileBookmark() {}
function EditBookmark() {}
function Newsgroups() {}
function AddressBook() 
{
	var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
	if (!toolkitCore)
	{
		toolkitCore = new ToolkitCore();
		if (toolkitCore)
		{
			toolkitCore.Init("ToolkitCore");
		}
    }

    if (toolkitCore)
	{
      toolkitCore.ShowWindow("chrome://addressbook/content/",
                             window);
    }

}

function History() {}
function SecurityInfo() {}
function MessengerCenter() {}
function JavaConsole() {}
function PageService() {}
function MailAccount() {}
function MaillingList() {}
function FolderPermission() {}
function ManageNewsgroup() {}
function WindowList() {}
function Help() {}
function About() {}
