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
	if ( selArray && (selArray.length == 1) )
	{
		var nextMessage = GetNextMessage(selArray[0], nextFunction, startFromBeginning);
		if(nextMessage)
		{
			tree.clearItemSelection();
			tree.selectItem(nextMessage);
		}
	}
}

/*GetNextMessage does the iterating for the Next menu item.
  currentMessage is the message we are starting from.  
  nextFunction is the function that will be used to detertime if a message matches criteria.
  It must take a node and return a boolean.
  startFromBeginning is a boolean that states whether or not we should start looking at the beginning
  if we reach then end 
*/
function GetNextMessage(currentMessage, nextFunction, startFromBeginning)
{
	var foundMessage = false;

	
	var nextMessage = currentMessage.nextSibling;
	while(nextMessage && (nextMessage != currentMessage))
	{
		if(nextFunction(nextMessage))
		 break;
		nextMessage = nextMessage.nextSibling;
		/*If there's no nextMessage we may have to start from top.*/
		if(!nextMessage && startFromBeginning)
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
		var previousMessage = GetPreviousMessage(selArray[0], previousFunction, startFromEnd);
		if(previousMessage)
		{
			tree.clearItemSelection();
			tree.selectItem(previousMessage);
		}
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

