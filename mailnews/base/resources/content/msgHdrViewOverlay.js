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

/* This is where functions related to displaying the headers for a selected message in the
   message pane live. */

////////////////////////////////////////////////////////////////////////////////////
// Warning: if you go to modify any of these JS routines please get a code review from either
// hangas@netscape.com or mscott@netscape.com. It's critical that the code in here for displaying
// the message headers for a selected message remain as fast as possible. In particular, 
// right now, we only introduce one reflow per message. i.e. if you click on a message in the thread
// pane, we batch up all the changes for displaying the header pane (to, cc, attachements button, etc.) 
// and we make a single pass to display them. It's critical that we maintain this one reflow per message
// view in the message header pane. 
//
////////////////////////////////////////////////////////////////////////////////////

var msgPaneData;
var currentHeaderData;

function OnLoadMsgHeaderPane()
{
   // build a document object for the header pane so we can cache fetches for elements
  // in a local variable
  // if you add more document elements with ids to msgHeaderPane.xul, you should add 
  // to this object...
  msgPaneData = new Object;
  currentHeaderData = new Object;

  if (msgPaneData)
  {
    // First group of headers
    msgPaneData.SubjectBox = document.getElementById("SubjectBox");
    msgPaneData.SubjectValue = document.getElementById("SubjectValue");
    msgPaneData.FromBox   =   document.getElementById("FromBox");
    msgPaneData.FromValue = document.getElementById("FromValue");
    msgPaneData.DateBox = document.getElementById("DateBox");
    msgPaneData.DateValue = document.getElementById("DateValue");

    // Attachment related elements
    msgPaneData.AttachmentPopup = document.getElementById("attachmentPopup");
    msgPaneData.AttachmentButton = document.getElementById("attachmentButton");

    // Second toolbar 
    msgPaneData.ToBox = document.getElementById("ToBox");
    msgPaneData.ToValue = document.getElementById("ToValue");
    msgPaneData.CcBox = document.getElementById("CcBox");
    msgPaneData.CcValue = document.getElementById("CcValue");
    msgPaneData.NewsgroupBox = document.getElementById("NewsgroupBox");
    msgPaneData.NewsgroupValue = document.getElementById("NewsgroupValue");

    // Third toolbar
    msgPaneData.UserAgentBox = document.getElementById("UserAgentBox");
    msgPaneData.UserAgentValue = document.getElementById("UserAgent");
  }

}

// The messageHeaderSink is the class that gets notified of a message's headers as we display the message
// through our mime converter. 

var messageHeaderSink = {
    onStartHeaders: function()
    {
      ClearCurrentHeaders();
      ClearAttachmentMenu();
    },

    onEndHeaders: function() 
    {
      // WARNING: This is the ONLY routine inside of the message Header Sink that should 
      // trigger a reflow!
      ShowMessageHeaderPane();
      UpdateMessageHeaders();
    },

    handleHeader: function(headerName, headerValue) 
    {
      // WARNING: if you are modifying this function, make sure you do not do *ANY*
      // dom manipulations which would trigger a reflow. This method gets called 
      // for every rfc822 header in the message being displayed. If you introduce a reflow
      // you'll be introducing a reflow for every time we are told about a header. And msgs have
      // a lot of headers. Don't do it =)....Move your code into OnEndHeaders which is only called
      // once per message view.

      // for consistancy sake, let's force all header names to be lower case so
      // we don't have to worry about looking for: Cc and CC, etc.
      headerName = headerName.toLowerCase();

      if (headerName == "subject")
      {
        currentHeaderData.SubjectValue = headerValue;
      }
      if (headerName == "from")
      {
        currentHeaderData.FromValue = headerValue;  
      }
      if (headerName == "date")
      {
        currentHeaderData.DateValue = headerValue; 
      }
      if (headerName == "to")
      {
         currentHeaderData.ToValue = headerValue; 
      }
      if (headerName == "cc")
      {
         currentHeaderData.CcValue = headerValue;  
      }
      if (headerName == "newsgroups")
      {
         currentHeaderData.NewsgroupsValue = headerValue;   
      }
      if (headerName == "user-agent")
      {
        currentHeaderData.UserAgentValue = headerValue;
      }
    },

    handleAttachment: function(url, displayName, uri) 
    {
      var commandString = "OpenAttachURL('" + url + "', '" + displayName + "', '" + uri + "')";
      AddAttachmentToMenu(displayName, commandString);
    }
};

function AddSenderToAddressBook() 
{
  // extract the from field from the current msg header and then call AddToAddressBook with that information
  var node = document.getElementById("FromValue");
  if (node)
  {
    var fromValue = node.childNodes[0].nodeValue;
    if (fromValue)
      AddToAddressBook(fromValue, "");
  }
}

function OpenAttachURL(url, displayName, messageUri)
{
  messenger.openAttachment(url, displayName, messageUri);
}

function AddAttachmentToMenu(name, oncommand) 
{ 
  var popup = document.getElementById("attachmentPopup"); 
  if ( popup && (popup.childNodes.length >= 2) ) 
  { 
    var item = document.createElement('menuitem'); 
    if ( item ) 
    { 
      item.setAttribute('value', name); 
      item.setAttribute('oncommand', oncommand); 
      var child = popup.childNodes[popup.childNodes.length - 2]; 
      popup.insertBefore(item, child); 
    } 

    var button = document.getElementById("attachmentButton");
    if (button)
       button.setAttribute("value", popup.childNodes.length - 2);
  } 

  var attachBox = document.getElementById("attachmentBox");
  if (attachBox)
    attachBox.removeAttribute("hide");
} 

function ClearAttachmentMenu() 
{ 
  var popup = document.getElementById("attachmentPopup"); 
  if ( popup ) 
  { 
     while ( popup.childNodes.length > 2 ) 
       popup.removeChild(popup.childNodes[0]); 
  } 

  var attachBox = document.getElementById("attachmentBox");
  if (attachBox)
    attachBox.setAttribute("hide", "true");
}

function UpdateMessageHeaders()
{
  hdrViewSetNodeWithBox(msgPaneData.SubjectBox, msgPaneData.SubjectValue, currentHeaderData.SubjectValue);
  hdrViewSetNodeWithBox(msgPaneData.FromBox, msgPaneData.FromValue, currentHeaderData.FromValue); 
  hdrViewSetNodeWithBox(msgPaneData.DateBox, msgPaneData.DateValue, currentHeaderData.DateValue); 
  hdrViewSetNodeWithBox(msgPaneData.ToBox, msgPaneData.ToValue, currentHeaderData.ToValue); 
  hdrViewSetNodeWithBox(msgPaneData.CcBox, msgPaneData.CcValue, currentHeaderData.CcValue);
  hdrViewSetNodeWithBox(msgPaneData.NewsgroupBox, msgPaneData.NewsgroupValue, currentHeaderData.NewsgroupsValue); 
  hdrViewSetNodeWithBox(msgPaneData.UserAgentBox, msgPaneData.UserAgentValue, currentHeaderData.UserAgentValue);
}

function ClearCurrentHeaders()
{
  currentHeaderData.SubjectValue = "";
  currentHeaderData.FromValue = "";
  currentHeaderData.DateValue = "";
  currentHeaderData.ToValue = "";
  currentHeaderData.CcValue = "";
  currentHeaderData.NewsgroupsValue = "";
  currentHeaderData.UserAgentValue = "";
}

function ShowMessageHeaderPane()
{ 
  var node = document.getElementById("headerPart1");
  if (node)
    node.removeAttribute("hide");
  node = document.getElementById("headerPart2");
  if (node)
    node.removeAttribute("hide");
  node = document.getElementById("headerPart3");
  if (node)
    node.removeAttribute("hide");
}

function HideMessageHeaderPane()
{
  var node = document.getElementById("headerPart1");
  if (node)
    node.setAttribute("hide", "true");
  node = document.getElementById("headerPart2");
  if (node)
    node.setAttribute("hide", "true");
  node = document.getElementById("headerPart3");
  if (node)
    node.setAttribute("hide", "true");
}

// The following are just small helper functions..
function hdrViewSetNodeWithBox(boxNode, textNode, text)
{
	if ( text )
		return hdrViewSetNode(boxNode, textNode, text );
	else
	{
		hdrViewSetVisible(boxNode, false);
		return false;
	}
}

function hdrViewSetNode(boxNode, textNode, text)
{
	textNode.childNodes[0].nodeValue = text;
	var visible;
	
	if ( text )
		visible = true;
	else
		visible = false;
	
	hdrViewSetVisible(boxNode, visible);
	return visible;
}

function hdrViewSetVisible(boxNode, visible)
{
	if ( visible )
		boxNode.removeAttribute("hide");
	else
		boxNode.setAttribute("hide", "true");
}
