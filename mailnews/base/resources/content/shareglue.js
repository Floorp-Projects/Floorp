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



function CloseMessenger() 
{
	dump("\nClose from XUL\nDo something...\n");
	messenger.Close();
}

function CharacterSet(){}

function MessengerSetDefaultCharacterSet(aCharset)
{
    dump(aCharset);dump("\n");
    messenger.SetDocumentCharset(aCharset);
	var folderResource = GetSelectedFolderResource();
	SetFolderCharset(folderResource, aCharset);
	RefreshThreadTreeView();
    MsgReload();
}

function Print() {
	dump("Print()\n");
	try {
		messenger.DoPrint();
	}
	catch (ex) {
		dump("failed to print\n");
	}
}
function PrintPreview() {
	dump("PrintPreview()\n");
	try {
		messenger.DoPrintPreview();
	}
	catch (ex) {
		dump("failed to print preview\n");
	}
}
