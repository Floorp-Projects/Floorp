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

var msgHeaderParserProgID		   = "component://netscape/messenger/headerparser";
var abAddressCollectorProgID	 = "component://netscape/addressbook/services/addressCollecter";

var msgPaneData;
var currentHeaderData = {};
var gNumAddressesToShow = 3;
var gShowUserAgent = false;

// attachments array
var attachmentUrlArray = new Array();
var attachmentDisplayNameArray = new Array();
var attachmentMessageUriArray = new Array();

var msgHeaderParser = Components.classes[msgHeaderParserProgID].getService(Components.interfaces.nsIMsgHeaderParser);
var abAddressCollector = Components.classes[abAddressCollectorProgID].getService(Components.interfaces.nsIAbAddressCollecter);

function OnLoadMsgHeaderPane()
{
   // build a document object for the header pane so we can cache fetches for elements
  // in a local variable
  // if you add more document elements with ids to msgHeaderPane.xul, you should add 
  // to this object...
  msgPaneData = new Object;
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
    
    // ToValueShort is the div which shows a shortened number of addresses
    // on the to line...ToValueLong is a div which shows all of the
    // addresses on the to line. The same rule applies for ccValueShort/Long
    msgPaneData.ToValueShort = document.getElementById("ToValueShort");
    msgPaneData.ToValueLong = document.getElementById("ToValueLong");
    msgPaneData.ToValueToggleIcon = document.getElementById("ToValueToggleIcon");

    msgPaneData.CcBox = document.getElementById("CcBox");
    msgPaneData.CcValueShort = document.getElementById("CcValueShort");
    msgPaneData.CcValueLong = document.getElementById("CcValueLong");
    msgPaneData.CcValueToggleIcon = document.getElementById("CcValueToggleIcon");

    msgPaneData.NewsgroupBox = document.getElementById("NewsgroupBox");
    msgPaneData.NewsgroupValue = document.getElementById("NewsgroupValue");

    msgPaneData.UserAgentBox = document.getElementById("UserAgentBox");
    msgPaneData.UserAgentValue = document.getElementById("UserAgentValue");

  }
  
  // load any preferences that at are global with regards to 
  // displaying a message...
  gNumAddressesToShow = pref.GetIntPref("mailnews.max_header_display_length");
  gShowUserAgent = pref.GetBoolPref("mailnews.headers.showUserAgent");
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
      
      NotifyClearAddresses();

      // (1) clear out the email fields for to, from, cc....
      ClearEmailField(msgPaneData.FromValue);
      ClearEmailFieldWithButton(msgPaneData.ToValueShort);
      ClearEmailFieldWithButton(msgPaneData.CcValueShort);
      ClearEmailFieldWithButton(msgPaneData.ToValueLong);
      ClearEmailFieldWithButton(msgPaneData.CcValueLong);

      // be sure to re-hide the toggle button, we'll re-enable it later if we need it...
      msgPaneData.ToValueToggleIcon.setAttribute('hideNonBox', 'true');
      msgPaneData.CcValueToggleIcon.setAttribute('hideNonBox', 'true');
      //hdrViewSetVisible(msgPaneData.ToValueToggleIcon, false);
      //hdrViewSetVisible(msgPaneData.CcValueToggleIcon, false);
      
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
      currentHeaderData[headerName] = headerValue;
      if (headerName == "from")
      {
        if (headerValue && abAddressCollector)
          abAddressCollector.collectUnicodeAddress(headerValue);  
      }
 
    },

    handleAttachment: function(contentType, url, displayName, uri, notDownloaded) 
    {
        // be sure to escape the display name before we insert it into the
        // method
      var commandString = "OpenAttachURL('" + contentType + "', '" + url + "', '" + escape(displayName) + "', '" + uri + "')";
      if (notDownloaded)
      {
        displayName += " " + Bundle.GetStringFromName("notDownloaded");
      }

      AddAttachmentToMenu(displayName, commandString);

      var count = attachmentUrlArray.length;
      // dump ("** attachment count**" + count + "\n");
      if (count < 0) count = 0;
      attachmentUrlArray[count] = url;
      attachmentDisplayNameArray[count] = escape(displayName);
      attachmentMessageUriArray[count] = uri;
    },
    
    onEndAllAttachments: function()
    {
        AddSaveAllAttachmentsMenu();
    }
};

function AddNodeToAddressBook (emailAddressNode)
{
}

// SendMailToNode takes the email address title button, extracts
// the email address we stored in there and opens a compose window
// with that address
function SendMailToNode(emailAddressNode)
{
  if (emailAddressNode)
  {
     var emailAddress = emailAddressNode.getAttribute("emailAddress");
     if (emailAddress)
        messenger.OpenURL("mailto:" + emailAddress );
  }
}

function AddSenderToAddressBook() 
{
  // extract the from field from the current msg header and then call AddToAddressBook with that information
  var node = document.getElementById("FromValue");
  if (node)
  {
    var fromValue = node.value; 
    // node.childNodes[0].nodeValue;
    if (fromValue)
      AddToAddressBook(fromValue, "");
  }
}

function OpenAttachURL(contentType, url, displayName, messageUri)
{
    var args = {dspname: displayName, opval: 0};

  window.openDialog("chrome://messenger/content/openSaveAttachment.xul",
                    "openSaveAttachment", "chrome,modal", args);
  if (args.opval == 1)
      messenger.openAttachment(contentType, url, displayName, messageUri);
  else if (args.opval == 2)
      messenger.saveAttachment(url, displayName, messageUri);
}

function AddAttachmentToMenu(name, oncommand) 
{ 
  var popup = document.getElementById("attachmentPopup"); 
  if (popup)
  { 
    var item = document.createElement('menuitem'); 
    if ( item ) 
    { 
      // popup.removeAttribute('menugenerated');

      popup.appendChild(item);
      item.setAttribute('value', name); 
      item.setAttribute('oncommand', oncommand); 
    } 

    var button = document.getElementById("attachmentButton");
    if (button)
       button.setAttribute("value", popup.childNodes.length);
  } 

  var attachBox = document.getElementById("attachmentBox");
  if (attachBox)
    attachBox.removeAttribute("hide");
} 

function SaveAllAttachments()
{
    try 
    {
        messenger.saveAllAttachments(attachmentUrlArray.length,
                                     attachmentUrlArray,
                                     attachmentDisplayNameArray,
                                     attachmentMessageUriArray);
    }
    catch (ex)
    {
        dump ("** failed to save all attachments ** \n");
    }
}

function AddSaveAllAttachmentsMenu()
{
    var popup = document.getElementById("attachmentPopup");
    if (popup && popup.childNodes.length > 1)
    {
        var separator = document.createElement('menuseparator');
        var item = document.createElement('menuitem');
        if (separator && item)
        {
            popup.appendChild(separator);
            popup.appendChild(item);
            item.setAttribute('value', "Save All...");
            item.setAttribute('oncommand', "SaveAllAttachments()");
        }
    }
}


function ClearAttachmentMenu() 
{ 
  var popup = document.getElementById("attachmentPopup"); 
  if ( popup ) 
  { 
     while ( popup.childNodes.length ) 
       popup.removeChild(popup.childNodes[0]); 
  } 

  var attachBox = document.getElementById("attachmentBox");
  if (attachBox)
    attachBox.setAttribute("hide", "true");

  // reset attachments name array
  attachmentUrlArray.length = 0;
  attachmentDisplayNameArray.length = 0;
  attachmentMessageUriArray.length = 0;
}

// Assumes that all the child nodes of the parent div need removed..leaving
// an empty parent div...This is used to clear the To/From/cc lines where
// we had to insert email addresses one at a time into their parent divs...
function ClearEmailFieldWithButton(parentDiv)
{
  if (parentDiv)
  {
    // the toggle button is the last child in the child nodes..
    // we should never remove it...
    while (parentDiv.childNodes.length > 1)
      parentDiv.removeChild(parentDiv.childNodes[0]);
  }
}

// Clear Email Field takes the passed in div and removes all the child nodes!
// if your div has a button in it (like To/cc use ClearEmailfieldWithButton
function ClearEmailField(parentDiv)
{
  if (parentDiv)
  {
    while (parentDiv.childNodes.length > 0)
      parentDiv.removeChild(parentDiv.childNodes[0]);
  }
}

// OutputEmailAddresses --> knows how to take a comma separated list of email addresses,
// extracts them one by one, linkifying each email address into a mailto url. 
// Then we add the link'ified email address to the parentDiv passed in.
// 
// defaultParentDiv --> the div to add the link-ified email addresses into. 
// emailAddresses --> comma separated list of the addresses for this header field
// includeShortLongToggle --> true if you want to include the ability to toggle between short/long
// address views for this header field. If true, then pass in a another div which is the div the long
// view will be added too...

function OutputEmailAddresses(parentBox, defaultParentDiv, emailAddresses, includeShortLongToggle, optionalLongDiv, optionalToggleButton)
{
  // if we don't have any addresses for this field, hide the parent box!
	if ( !emailAddresses )
	{
		hdrViewSetVisible(parentBox, false);
		return;
	}
  if (msgHeaderParser)
  {
    var enumerator = msgHeaderParser.ParseHeadersWithEnumerator(emailAddresses);
    enumerator = enumerator.QueryInterface(Components.interfaces.nsISimpleEnumerator);
    var numAddressesParsed = 0;
    if (enumerator)
    {
      var emailAddress = {};
      var name = {};

      while (enumerator.HasMoreElements())
      {
        var headerResult = enumerator.GetNext();
        headerResult = enumerator.QueryInterface(Components.interfaces.nsIMsgHeaderParserResult);
        
        // get the email and name fields
        var addrValue = {};
        var nameValue = {};
        fullAddress = headerResult.getAddressAndName(addrValue, nameValue);
        emailAddress = addrValue.value;
        name = nameValue.value;

        // turn the strings back into a full address
        // var fullAddress = msgHeaderParser.MakeFullAddress(name, emailAddress);

        // if we want to include short/long toggle views and we have a long view, always add it.
        // if we aren't including a short/long view OR if we are and we haven't parsed enough
        // addresses to reach the cutoff valve yet then add it to the default (short) div.
        if (includeShortLongToggle && optionalLongDiv)
        {
          InsertEmailAddressUnderEnclosingBox(parentBox, optionalLongDiv, emailAddress, fullAddress);
        }
        if (!includeShortLongToggle || (numAddressesParsed < gNumAddressesToShow))
        {
          InsertEmailAddressUnderEnclosingBox(parentBox, defaultParentDiv, emailAddress, fullAddress);
        }
        
        numAddressesParsed++;
      } 
    } // if enumerator

    if (includeShortLongToggle && (numAddressesParsed > gNumAddressesToShow) && optionalToggleButton)
    {
      optionalToggleButton.removeAttribute('hideNonBox');
    }
  } // if msgheader parser
}

/* InsertEmailAddressUnderEnclosingBox --> right now all email addresses are borderless titled buttons
   with formatting to make them look like html anchors. When you click on the button,
   you are prompted with a popup asking you what you want to do with the email address

   parentBox --> the enclosing box for all the email addresses for this header. This is needed
                 to control visibility of the header.
   parentDiv --> the DIV the email addresses need to be inserted into.
*/
   
function InsertEmailAddressUnderEnclosingBox(parentBox, parentDiv, emailAddress, fullAddress) 
{
  if ( parentBox ) 
  { 
    var item = document.createElement("titledbutton");
    if ( item && parentDiv) 
    { 
      item.setAttribute("class", "emailDisplayButton");
      item.setAttribute("value", fullAddress); 

      if (parentDiv.childNodes.length)
      {
        var child = parentDiv.childNodes[parentDiv.childNodes.length - 1]; 
        if (parentDiv.childNodes.length > 1)
        {
          var textNode = document.createElement("text");
          textNode.setAttribute("value", ", ");
          parentDiv.insertBefore(textNode, child);       
        }
        parentDiv.insertBefore(item, child);
      }
      else
       parentDiv.appendChild(item);
      
      item.setAttribute("class", "emailDisplayButton");
      item.setAttribute("popup", "emailAddressPopup");
      item.setAttribute("value", fullAddress);
      
      item.setAttribute("emailAddress", emailAddress);
      item.setAttribute("fullAddress", fullAddress);  

      AddExtraAddressProcessing(emailAddress, item);

      hdrViewSetVisible(parentBox, true);
    } 
  } 
}


function UpdateMessageHeaders()
{
  hdrViewSetNodeWithBox(msgPaneData.SubjectBox, msgPaneData.SubjectValue, currentHeaderData["subject"]);
  OutputEmailAddresses(msgPaneData.FromBox, msgPaneData.FromValue, currentHeaderData["from"], false, "", ""); 
  hdrViewSetNodeWithBox(msgPaneData.DateBox, msgPaneData.DateValue, currentHeaderData["date"]); 
  OutputEmailAddresses(msgPaneData.ToBox, msgPaneData.ToValueShort, currentHeaderData["to"], true, msgPaneData.ToValueLong, msgPaneData.ToValueToggleIcon );
  OutputEmailAddresses(msgPaneData.CcBox, msgPaneData.CcValueShort, currentHeaderData["cc"], true, msgPaneData.CcValueLong, msgPaneData.CcValueToggleIcon );
  hdrViewSetNodeWithBox(msgPaneData.NewsgroupBox, msgPaneData.NewsgroupValue, currentHeaderData["newsgroups"]); 

  if (gShowUserAgent)
    hdrViewSetNodeWithBox(msgPaneData.UserAgentBox, msgPaneData.UserAgentValue, currentHeaderData["user-agent"]);
  FinishEmailProcessing();
}

function ClearCurrentHeaders()
{
  currentHeaderData = {};
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

// ToggleLongShortAddresses is used to toggle between showing
// all of the addresses on a given header line vs. only the first 'n'
// where 'n' is a user controlled preference. By toggling on the more/less
// images in the header window, we'll hide / show the appropriate div for that header.

function ToggleLongShortAddresses(shortDivID, longDivID)
{
  var shortNode = document.getElementById(shortDivID);
  var longNode = document.getElementById(longDivID);

  // test to see which if short is already hidden...
  if (shortNode.getAttribute("hide") == "true")
  {
     hdrViewSetVisible(longNode, false);
     hdrViewSetVisible(shortNode, true);
  }
  else
  {
     hdrViewSetVisible(shortNode, false);
     hdrViewSetVisible(longNode, true);
  }
}

///////////////////////////////////////////////////////////////
// The following are just small helper functions..
///////////////////////////////////////////////////////////////

function hdrViewSetNodeWithButton(boxNode, buttonNode, text)
{
  if (text)
  {
    buttonNode.setAttribute("value", text);
    hdrViewSetVisible(boxNode, true);
  }
	else
	{
		hdrViewSetVisible(boxNode, false);
		return false;
	}
} 

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

