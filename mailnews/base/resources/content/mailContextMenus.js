/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

 function fillThreadPaneContextMenu()
{
	var tree = GetThreadTree();
	var selectedItems = tree.selectedItems;
	var numSelected = selectedItems.length;

	ShowMenuItem("threadPaneContext-openNewWindow", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-openNewWindow", false);

	ShowMenuItem("threadPaneContext-editAsNew", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-editAsNew", false);
	
	ShowMenuItem("threadPaneContext-sep-open", (numSelected <= 1));

	ShowMenuItem("threadPaneContext-replySender", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-replySender", (numSelected == 1));

	ShowMenuItem("threadPaneContext-replyAll", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-replyAll", (numSelected == 1));

	ShowMenuItem("threadPaneContext-forward", true);
	EnableMenuItem("threadPaneContext-forward", (numSelected > 0));

	ShowMenuItem("threadPaneContext-sep-reply", true);

	ShowMenuItem("threadPaneContext-moveMenu", true);
	EnableMenuItem("threadPaneContext-moveMenu", (numSelected > 0));

	ShowMenuItem("threadPaneContext-copyMenu", true);
	EnableMenuItem("threadPaneContext-copyMenu", (numSelected > 0));

	ShowMenuItem("threadPaneContext-saveAs", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-saveAs", (numSelected == 1));

	ShowMenuItem("threadPaneContext-print", true);
	EnableMenuItem("threadPaneContext-print", (numSelected > 0));

	ShowMenuItem("threadPaneContext-delete", true);
	EnableMenuItem("threadPaneContext-delete", (numSelected > 0));

	ShowMenuItem("threadPaneContext-sep-edit", (numSelected <= 1));

	ShowMenuItem("threadPaneContext-addSenderToAddressBook", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-addSenderToAddressBook", false);

	ShowMenuItem("threadPaneContext-addAllToAddressBook", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-addAllToAddressBook", false);

	return(true);
}

function ShowMenuItem(id, showItem)
{
	var item = document.getElementById(id);
	var showing = (item.getAttribute('hidden') !='true');
	if(item && (showItem != showing))
		item.setAttribute('hidden', showItem ? '' : 'true');
}

function EnableMenuItem(id, enableItem)
{
	var item = document.getElementById(id);
	var enabled = (item.getAttribute('disabled') !='true');
	if(item && (enableItem != enabled))
	{
		item.setAttribute('disabled', enableItem ? '' : 'true');
	}
}

