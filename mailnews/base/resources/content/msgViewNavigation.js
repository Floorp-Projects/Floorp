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

function ResourceGoMessage(message)
{
	return true;
}

function GoUnreadMessage(message)
{
	var isUnread = message.getAttribute('IsUnread');
	return(isUnread == 'true');
}

function ResourceGoUnreadMessage(message)
{
	var isUnreadValue = GetMessageValue(message, "http://home.netscape.com/NC-rdf#IsUnread");
	return(isUnreadValue == 'true');
}

function GoFlaggedMessage(message)
{
	var flagged = message.getAttribute('Flagged');
	return(flagged == 'flagged');
}

function ResourceGoFlaggedMessage(message)
{
	var flaggedValue = GetMessageValue(message, "http://home.netscape.com/NC-rdf#Flagged");
	return(flaggedValue == 'flagged');
}

function GetMessageValue(message, propertyURI)
{
	var db = GetThreadTree().database;
	var propertyResource = RDF.GetResource(propertyURI);
	var node = db.GetTarget(message, propertyResource, true);
	var literal = node.QueryInterface(Components.interfaces.nsIRDFLiteral);
	if(literal)
	{
		return literal.Value;
	}
	return null;
}

function GoUnreadThread(messageElement)
{
	var messageuri = messageElement.getAttribute('id');
	var messageResource = RDF.GetResource(messageuri);

	var unreadCount = GetMessageValue(messageResource, "http://home.netscape.com/NC-rdf#TotalUnreadMessages");
	return (unreadCount !="");
}

/*GoNextMessage finds the message that matches criteria and selects it.  
  nextFunction is the function that will be used to detertime if a message matches criteria.
  It must take a node and return a boolean.
  nextResourceFunction is the function that will be used to determine if a message in the form of a resource
  matches criteria.  Takes a resource and returns a boolean
  nextThreadFunction is an optional function that can be used to optimize whether or not a thread will have a
  message that matches the criteria.  Takes the top level message in the form of a node and returns a boolean.
  startFromBeginning is a boolean that states whether or not we should start looking at the beginning
  if we reach the end 
*/
function GoNextMessage(nextFunction, nextResourceFunction, nextThreadFunction, startFromBeginning)
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

		var nextMessage;

		if(messageView.showThreads)
		{
			nextMessage = GetNextMessageInThreads(tree, currentMessage, nextFunction, nextResourceFunction, nextThreadFunction, startFromBeginning);	
		}
		else
		{
			nextMessage = GetNextMessage(tree, currentMessage, nextFunction, startFromBeginning);
		}

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

function GetNextMessageInThreads(tree, currentMessage, nextFunction, nextResourceFunction, nextThreadFunction, startFromBeginning)
{
	var checkStartMessage = false;

	//In the case where nothing is selected
	if(currentMessage == null)
	{
		currentMessage = FindFirstMessage(tree);
		checkStartMessage = true;
	}

	return FindNextMessageInThreads(currentMessage, currentMessage, nextFunction, nextResourceFunction, nextThreadFunction, startFromBeginning, checkStartMessage);
}

function FindNextMessageInThreads(startMessage, originalStartMessage, nextFunction, nextResourceFunction, nextThreadFunction, startFromBeginning, checkStartMessage)
{
	var nextMessage;
	var nextChildMessage;

	//First check startMessage if we are supposed to
	if(checkStartMessage)
	{
		if(nextFunction(startMessage))
			return startMessage;
	}

	//if we're on the top level and a thread function has been passed in, we might be able to search faster.
	if(startMessage.parentNode.parentNode.nodeName != "treeitem" && nextThreadFunction)
	{
		var nextTopMessage = FindNextThread(startMessage, nextThreadFunction, startFromBeginning, true);
		nextMessage = GetNextInThread(nextTopMessage, nextFunction, nextResourceFunction);
		if(nextMessage)
		{
			return nextMessage;
		}
	}

	//Next, search the current messages children.
	nextChildMessage = FindNextInChildren(startMessage, originalStartMessage, nextFunction, nextResourceFunction);
	if(nextChildMessage)
		return nextChildMessage;

	//Next we need to search the current messages siblings
	nextMessage = startMessage.nextSibling;
	while(nextMessage)
	{
		//In case we've already been here before
		if(nextMessage == originalStartMessage)
			return nextMessage;

		if(nextFunction(nextMessage))
			return nextMessage;

		var nextChildMessage = FindNextInChildren(nextMessage, originalStartMessage, nextFunction, nextResourceFunction);
		if(nextChildMessage)
			return nextChildMessage;
			 
		nextMessage = nextMessage.nextSibling;
	}

	//Finally, we need to find the next of the start message's ancestors that has a sibling
	var parentMessage = startMessage.parentNode.parentNode;

	while(parentMessage.nodeName == 'treeitem')
	{
		if(parentMessage.nextSibling != null)
		{
			nextMessage = FindNextMessageInThreads(parentMessage.nextSibling, originalStartMessage, nextFunction, nextResourceFunction, nextThreadFunction, startFromBeginning, true);
			return nextMessage;
		}
		parentMessage = parentMessage.parentNode.parentNode;
	}
	//otherwise it's the tree so we need to stop and potentially start from the beginning
	if(startFromBeginning)
	{
		nextMessage = FindNextMessageInThreads(FindFirstMessage(parentMessage), originalStartMessage, nextFunction, nextResourceFunction, nextThreadFunction, false, true);
		return nextMessage;
	}
	return null;

}

//Searches children messages in thread navigation.
function FindNextInChildren(parentMessage, originalStartMessage, nextFunction, nextResourceFunction)
{
	var isParentOpen = parentMessage.getAttribute('open') == 'true';
	//First we'll deal with the case where the parent is open.  In this case we can use DOM calls.
	if(isParentOpen)
	{
		//In this case we have treechildren
		if(parentMessage.childNodes.length == 2)
		{
			var treechildren = parentMessage.childNodes[1];
			var childMessages = treechildren.childNodes;
			var numChildMessages = childMessages.length;

			for(var i = 0; i < numChildMessages; i++)
			{
				var childMessage = childMessages[i];

				//If we're at the original message again then stop.
				if(childMessage == originalStartMessage)
					return childMessage;

				if(nextFunction(childMessage))
					return childMessage;
				else
				{
					//if this child isn't the message, perhaps one of its children is.
					var nextChildMessage = FindNextInChildren(childMessage, originalStartMessage, nextFunction);
					if(nextChildMessage)
						return nextChildMessage;
				}
			}
		}
	}
	else
	{
		//We need to traverse the graph in rdf looking for the next resource that fits what we're searching for.
		var parentUri = parentMessage.getAttribute('id');
		var parentResource = RDF.GetResource(parentUri);

		//If we find one, then we get the id and open up the parent and all of it's children.  Then we find the element
		//with the id in the document and return that.
		if(parentResource)
		{
			var nextResource = FindNextInChildrenResources(parentResource, nextResourceFunction);
			if(nextResource)
			{
				OpenThread(parentMessage);
				var nextUri = nextResource.Value;
				var nextChildMessage = document.getElementById(nextUri);
				return nextChildMessage;
			}
		}

	}
	return null;
}

function FindNextInChildrenResources(parentResource, nextResourceFunction)
{
	var db = GetThreadTree().database;

	var childrenResource = RDF.GetResource("http://home.netscape.com/NC-rdf#MessageChild");
	var childrenEnumerator = db.GetTargets(parentResource, childrenResource, true);

	if(childrenEnumerator)
	{
		while(childrenEnumerator.HasMoreElements())
		{
			var childResource = childrenEnumerator.GetNext().QueryInterface(Components.interfaces.nsIRDFResource);
			if(childResource)
			{
				if(nextResourceFunction(childResource))
					return childResource;

				var nextMessageResource = FindNextInChildrenResources(childResource, nextResourceFunction);
				if(nextMessageResource)
					return nextMessageResource;

			}


		}

	}

	return null;
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
	//getElementsByTagName is too slow which is why I'm using this loop.  Just find the first
	//child of the tree that has the 'treechildren' tag and return it's first child.  This will
	//be the first message.

	var children = tree.childNodes;
	var numChildren = children.length;

	for(var i = 0; i < numChildren; i++)
	{
		if(children[i].nodeName == 'treechildren')
		{
			return children[i].firstChild;
		}

	}

	return null;	

}

// nextThreadFunction is the function that determines whether a top level message is part of a thread that fits criteria.
// nextMessageFunction is the function that would be used to find the next message in a thread if gotoNextInThread is true
// nextResourceFunction is the function that would be used to find the next message in a thread as a resource if gotoNextInThread is true
// startFromBeginning is true if we should start looking from the beginning after we get to the end of the thread pane.
// gotoNextInThread is true if once we find an unrad thread we should select the first message in that thread that fits criteria
function GoNextThread(nextThreadFunction, nextMessageFunction, nextResourceFunction, startFromBeginning, gotoNextInThread)
{

	if(messageView.showThreads)
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

			var nextMessage;
			var currentTopMessage;
			var checkCurrentTopMessage;
			//Need to get the parent message for the current selection to begin to find thread
			if(currentMessage)
			{
				//need to find its top level message and we don't want it to be checked for criteria
				currentTopMessage = FindTopLevelMessage(currentMessage);
				checkCurrentTopMessage = false;
			}
			else
			{
				//currentTopmessage is the first one in the tree and we want it to be checked for criteria.
				currentTopMessage = FindFirstMessage(tree);
				checkCurrentTopMessage = true;
			}

			var nextTopMessage = FindNextThread(currentTopMessage, nextThreadFunction, startFromBeginning, checkCurrentTopMessage);
			var changeSelection = (nextTopMessage != null && ((currentTopMessage != nextTopMessage) || checkCurrentTopMessage));
			if(changeSelection)
			{
				if(gotoNextInThread)
				{
					nextMessage = GetNextInThread(nextTopMessage, nextMessageFunction, nextResourceFunction);
					ChangeSelection(tree, nextMessage);
				}
				else
					ChangeSelection(tree, nextTopMessage);	
			}
		}
	}

}

//Given the top level message of a thread and the searching functions, this returns the first message in that thread that matches
//the criteria
function GetNextInThread(topMessage, nextMessageFunction, nextResourceFunction)
{
	var nextMessage;

	if(nextMessageFunction(topMessage))
		nextMessage = topMessage;
	else
	{
		nextMessage = FindNextInChildren(topMessage, null, nextMessageFunction, nextResourceFunction);
	}

	return nextMessage;
}

function FindTopLevelMessage(startMessage)
{
	var currentTop = startMessage;
	var parent = startMessage.parentNode.parentNode;

	while(parent.nodeName == 'treeitem')
	{
		currentTop = parent;
		parent = parent.parentNode.parentNode;
	}

	return currentTop;
}

function FindNextThread(startThread, nextThreadFunction, startFromBeginning, checkStartThread)
{
	if(checkStartThread)
	{
		if(nextThreadFunction(startThread))
			return startThread;
		dump("start thread doesn't match\n");
	}

	var nextThread = startThread.nextSibling;

	//In case we are currently the bottom message
	if(!nextThread && startFromBeginning)
	{
		var parent = startThread.parentNode;
		nextThread = parent.firstChild;
	}


	while(nextThread && (nextThread != startThread))
	{
		if(nextThreadFunction(nextThread))
		{
		 break;
		}
		nextThread = nextThread.nextSibling;
		/*If there's no nextMessage we may have to start from top.*/
		if(!nextThread && (nextThread!= startThread) && startFromBeginning)
		{
			var parent = startThread.parentNode;
			nextThread = parent.firstChild;
		}
	}

	return nextThread;
}



