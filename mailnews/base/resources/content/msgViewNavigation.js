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

/*  This file contains the js functions necessary to implement view navigation within the 3 pane. */

function GoMessage(message)
{
	return true;
}

function GoUnreadMessage(message)
{
	var status = message.getAttribute('Status');
	return(status == ' ' || status == 'New');
}

function GoFlaggedMessage(message)
{
	var flagged = message.getAttribute('Flagged');
	return(flagged == 'flagged');
}


/*GoNextMessage finds the message that matches criteria and selects it.  
  nextFunction is the function that will be used to detertime if a message matches criteria.
  It must take a node and return a boolean.
  startFromBeginning is a boolean that states whether or not we should start looking at the beginning
  if we reach then end 
*/
function GoNextMessage(nextFunction, startFromBeginning)
{
	var tree = GetThreadTree();
	
	var selArray = tree.selectedItems;
	var length = selArray.length;

	if ( selArray && ((length == 0) || (length == 1)) )
	{
		var currentMessage;

		if(length == 0)
			currentMessage = null;
		else
			currentMessage = selArray[0];

		var nextMessage = GetNextMessage(tree, currentMessage, nextFunction, startFromBeginning);

		//Only change the selection if there's a valid nextMessage
		if(nextMessage && (nextMessage != currentMessage))
			ChangeSelection(tree, nextMessage);
	}
}

/*GetNextMessage does the iterating for the Next menu item.
  currentMessage is the message we are starting from.  
  nextFunction is the function that will be used to detertime if a message matches criteria.
  It must take a node and return a boolean.
  startFromBeginning is a boolean that states whether or not we should start looking at the beginning
  if we reach then end 
*/
function GetNextMessage(tree, currentMessage, nextFunction, startFromBeginning)
{
	var foundMessage = false;

	var nextMessage;

	if(currentMessage)
		nextMessage = currentMessage.nextSibling;
	else
		nextMessage = FindFirstMessage(tree);

	//In case we are currently the bottom message
	if(!nextMessage && startFromBeginning)
	{
		dump('no next message\n');
		var parent = currentMessage.parentNode;
		nextMessage = parent.firstChild;
	}


	while(nextMessage && (nextMessage != currentMessage))
	{
		if(nextFunction(nextMessage))
		 break;
		nextMessage = nextMessage.nextSibling;
		/*If there's no nextMessage we may have to start from top.*/
		if(!nextMessage && (nextMessage!= currentMessage) && startFromBeginning)
		{
			dump('We need to start from the top\n');
			var parent = currentMessage.parentNode;
			nextMessage = parent.firstChild;
		}
	}

	if(nextMessage)
	{
		var id = nextMessage.getAttribute('id');
		dump(id + '\n');
	}
	else
		dump('No next message\n');
	return nextMessage;
}


/*GoPreviousMessage finds the message that matches criteria and selects it.  
  previousFunction is the function that will be used to detertime if a message matches criteria.
  It must take a node and return a boolean.
  startFromEnd is a boolean that states whether or not we should start looking at the end
  if we reach the beginning 
*/
function GoPreviousMessage(previousFunction, startFromEnd)
{
	var tree = GetThreadTree();
	
	var selArray = tree.selectedItems;
	if ( selArray && (selArray.length == 1) )
	{
		var currentMessage = selArray[0];
		var previousMessage = GetPreviousMessage(currentMessage, previousFunction, startFromEnd);
		//Only change selection if there's a valid previous message.
		if(previousMessage && (previousMessage != currentMessage))
			ChangeSelection(tree, previousMessage);
	}
}

/*GetPreviousMessage does the iterating for the Previous menu item.
  currentMessage is the message we are starting from.  
  previousFunction is the function that will be used to detertime if a message matches criteria.
  It must take a node and return a boolean.
  startFromEnd is a boolean that states whether or not we should start looking at the end
  if we reach then beginning 
*/
function GetPreviousMessage(currentMessage, previousFunction, startFromEnd)
{
	var foundMessage = false;

	
	var previousMessage = currentMessage.previousSibling;

	//In case we're already at the top
	if(!previousMessage && startFromEnd)
	{
		var parent = currentMessage.parentNode;
		previousMessage = parent.lastChild;
	}

	while(previousMessage && (previousMessage != currentMessage))
	{
		if(previousFunction(previousMessage))
		 break;
		previousMessage = previousMessage.previousSibling;
		if(!previousMessage && startFromEnd)
		{
			var parent = currentMessage.parentNode;
			previousMessage = parent.lastChild;
		}
	}

	if(previousMessage)
	{
		var id = previousMessage.getAttribute('id');
		dump(id + '\n');
	}
	else
		dump('No previous message\n');
	return previousMessage;
}

function ChangeSelection(tree, newMessage)
{
	if(newMessage)
	{
		tree.clearItemSelection();
		tree.clearCellSelection();
		tree.selectItem(newMessage);
	}
}

function FindFirstMessage(tree)
{
	var treeChildren = tree.getElementsByTagName('treechildren');
	if(treeChildren.length > 1)
	{
		//The first treeChildren will be for the template.
		return treeChildren[1].firstChild;

	}

	return null;	

}

