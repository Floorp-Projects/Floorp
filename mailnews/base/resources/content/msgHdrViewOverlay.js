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

var msgHeaderParserContractID		   = "@mozilla.org/messenger/headerparser;1";
var abAddressCollectorContractID	 = "@mozilla.org/addressbook/services/addressCollecter;1";

var msgPaneData;
var currentHeaderData = {};
// gGeneratedViewAllHeaderInfo --> we clear this every time we start to display a new message.
// the view all header popup will set it when we first generate a view of all the headers. this is 
// just so it won't try to regenerate all the information every time the user clicks on the popup.
var gGeneratedViewAllHeaderInfo = false; 
var gViewAllHeaders = false;
var gNumAddressesToShow = 3;
var gShowUserAgent = false;

// attachments array
var attachmentUrlArray = new Array();
var attachmentDisplayNameArray = new Array();
var attachmentMessageUriArray = new Array();

var msgHeaderParser = Components.classes[msgHeaderParserContractID].getService(Components.interfaces.nsIMsgHeaderParser);
var abAddressCollector = Components.classes[abAddressCollectorContractID].getService(Components.interfaces.nsIAbAddressCollecter);

// All these variables are introduced to keep track of insertion and deletion of the toggle button either
// as the first node in the list of emails or the last node.
var numOfEmailsInEnumerator;
var numOfEmailsInToField = 0;
var numOfEmailsInCcField = 0;
var numOfEmailsInFromField = 0;

// var used to determine whether to show the toggle button at the
// beginning or at the end of a list of emails in to/cc fields.
var gNumOfEmailsToShowToggleButtonInFront = 15;

function OnLoadMsgHeaderPane()
{
   // build a document object for the header pane so we can cache fetches for elements
  // in a local variable
  // if you add more document elements with ids to msgHeaderPane.xul, you should add 
  // to this object...
  msgPaneData = new Object;

  // HACK...force our XBL bindings file to be load before we try to create our first xbl widget....
  // otherwise we have problems.

  document.loadBindingDocument('chrome://messenger/content/mailWidgets.xml');

  if (msgPaneData)
  {
    // First group of headers
    msgPaneData.SubjectBox = document.getElementById("SubjectBox");
    msgPaneData.SubjectValue = document.getElementById("SubjectValue");
    msgPaneData.FromBox   =   document.getElementById("FromBox");
    msgPaneData.FromValue = document.getElementById("FromValue");
    msgPaneData.ReplyToBox   =   document.getElementById("ReplyToBox");
    msgPaneData.ReplyToValue = document.getElementById("ReplyToValue");

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
    msgPaneData.FollowupToBox = document.getElementById("FollowupToBox");
    msgPaneData.FollowupToValue = document.getElementById("FollowupToValue");

    msgPaneData.UserAgentBox = document.getElementById("UserAgentBox");
    msgPaneData.UserAgentToolbar = document.getElementById("headerPart3");
    msgPaneData.UserAgentValue = document.getElementById("UserAgentValue");

    msgPaneData.ViewAllHeadersToolbar = document.getElementById("viewAllHeadersToolbar");
    msgPaneData.ViewAllHeadersBox = document.getElementById("viewAllHeadersBox");
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
      // every time we start to redisplay a message, check the view all headers pref....
      var showAllHeadersPref = pref.GetIntPref("mail.show_headers");
      if (showAllHeadersPref == 2)
        gViewAllHeaders = true;
      else
        gViewAllHeaders = false;

      ClearCurrentHeaders();
      gGeneratedViewAllHeaderInfo = false;
      ClearAttachmentMenu();
    },

    onEndHeaders: function() 
    {
      // WARNING: This is the ONLY routine inside of the message Header Sink that should 
      // trigger a reflow!
      
      if (this.NotifyClearAddresses != undefined)
        NotifyClearAddresses();

      // (1) clear out the email fields for to, from, cc....
      ClearEmailField(msgPaneData.FromValue);
      ClearEmailField(msgPaneData.ReplyToValue);

      // When calling the ClearEmailFieldWithButton, pass in the name of the header too.
      ClearEmailFieldWithButton(msgPaneData.ToValueShort, "to");
      ClearEmailFieldWithButton(msgPaneData.CcValueShort, "cc");
      ClearEmailFieldWithButton(msgPaneData.ToValueLong, "to");
      ClearEmailFieldWithButton(msgPaneData.CcValueLong, "cc");

      // be sure to re-hide the toggle button, we'll re-enable it later if we need it...
      msgPaneData.ToValueToggleIcon.setAttribute('collapsed', 'true');
      msgPaneData.CcValueToggleIcon.setAttribute('collapsed', 'true');
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
      var lowerCaseHeaderName = headerName.toLowerCase();
      var foo = new Object;
      foo.headerValue = headerValue;
      foo.headerName = headerName;
      currentHeaderData[lowerCaseHeaderName] = foo;
      if (lowerCaseHeaderName == "from")
      {
 		    var  collectIncoming = pref.GetBoolPref("mail.collect_email_address_incoming");
        if (collectIncoming && headerValue && abAddressCollector)
          abAddressCollector.collectUnicodeAddress(headerValue);  
      }
 
    },

    handleAttachment: function(contentType, url, displayName, uri, notDownloaded) 
    {
        // be sure to escape the display name before we insert it into the
        // method
      var commandString = "OpenAttachURL('" + contentType + "', '" + url + "', '" + escape(displayName) + "', '" + uri + "')";
      var screenDisplayName = displayName;

      if (notDownloaded)
      {
        screenDisplayName += " " + Bundle.GetStringFromName("notDownloaded");
      }

      AddAttachmentToMenu(screenDisplayName, commandString);

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
  if (emailAddressNode)
  {
    var primaryEmail = emailAddressNode.getAttribute("emailAddress");
    var displayName = emailAddressNode.getAttribute("displayName");
	  window.openDialog("chrome://messenger/content/addressbook/abNewCardDialog.xul",
					  "",
					  "chrome,titlebar,resizeable=no", 
            {primaryEmail:primaryEmail, displayName:displayName });
  }
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
      popup.appendChild(item);
      item.setAttribute('value', name); 
      item.setAttribute('oncommand', oncommand); 
    } 

    var button = document.getElementById("attachmentButton");
    if (button)
    {
       button.setAttribute("value", popup.childNodes.length);
    }
  } 

  var attachBox = document.getElementById("attachmentBox");
  if (attachBox)
    attachBox.removeAttribute("collapsed");
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
    attachBox.setAttribute("collapsed", "true");

  // reset attachments name array
  attachmentUrlArray.length = 0;
  attachmentDisplayNameArray.length = 0;
  attachmentMessageUriArray.length = 0;
}

// Assumes that all the child nodes of the parent div need removed..leaving
// an empty parent div...This is used to clear the To/From/cc lines where
// we had to insert email addresses one at a time into their parent divs...
function ClearEmailFieldWithButton(parentDiv, headerName)
{
  if (parentDiv)
  {
    // the toggle button could be the very first child (if there are many email addresses)
    // or it could be the last child in the child nodes..
    // we should never remove the toggle button...

    // We use the delIndex variable to keep track of whether the toggle button is the:
    // first child -- in which case it is set to 1 and first element is never deleted
    // last child -- in which case it is set to 0 and the last element is never deleted.
    var delIndex = 1;

    // Depending on the headerName and the numOfEmails inserted into that header,
    // decide what the delIndex should be.
    if (headerName == "to") {
        if (numOfEmailsInToField < gNumOfEmailsToShowToggleButtonInFront)
            delIndex = 0;
    }
    else if (headerName == "cc") {
        if (numOfEmailsInCcField < gNumOfEmailsToShowToggleButtonInFront)
            delIndex = 0;
    }
    else {
        if (numOfEmailsInFromField < gNumOfEmailsToShowToggleButtonInFront)
            delIndex = 0;
    }

    while (parentDiv.childNodes.length > 1)
      parentDiv.removeChild(parentDiv.childNodes[delIndex]);
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

function OutputNewsgroups(boxNode, textNode, currentHeaderData, headerName)
{
  if (currentHeaderData[headerName])
  {    
    var headerValue = currentHeaderData[headerName].headerValue;
    headerValue = headerValue.replace(/,/g,", ");

    hdrViewSetNodeWithBox(boxNode, textNode, headerValue);
  }
  else
    hdrViewSetNodeWithBox(boxNode, textNode, "");
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

function OutputEmailAddresses(parentBox, defaultParentDiv, emailAddresses, includeShortLongToggle, optionalLongDiv, optionalToggleButton, currentHeaderName)
{
  // if we don't have any addresses for this field, hide the parent box!
	if ( !emailAddresses )
	{
		hdrViewSetVisible(parentBox, false);
		return;
	}
  if (msgHeaderParser)
  {
    // Count the number of email addresses being inserted into each header
    // The headers could be "to", "cc", "from"
    var myEnum = msgHeaderParser.ParseHeadersWithEnumerator(emailAddresses);
    myEnum = myEnum.QueryInterface(Components.interfaces.nsISimpleEnumerator);

    numOfEmailsInEnumerator = 0;
    while (myEnum.hasMoreElements())
    {
        myEnum.getNext();
        numOfEmailsInEnumerator++;
    }

    // Depending on the headerName and the variable that keeps track of the
    // numOfEmails inserted into that header,
    if (currentHeaderName == "to")
        numOfEmailsInToField = numOfEmailsInEnumerator;
    else if (currentHeaderName == "cc")
        numOfEmailsInCcField = numOfEmailsInEnumerator;
    else if (currentHeaderName == "from") 
        numOfEmailsInFromField = numOfEmailsInEnumerator;

    var enumerator = msgHeaderParser.ParseHeadersWithEnumerator(emailAddresses);
    enumerator = enumerator.QueryInterface(Components.interfaces.nsISimpleEnumerator);
    var numAddressesParsed = 0;
    if (enumerator)
    {
      var emailAddress = {};
      var name = {};

      while (enumerator.hasMoreElements())
      {
        var headerResult = enumerator.getNext();
        headerResult = enumerator.QueryInterface(Components.interfaces.nsIMsgHeaderParserResult);
        
        // get the email and name fields
        var addrValue = {};
        var nameValue = {};
        var fullAddress = headerResult.getAddressAndName(addrValue, nameValue);
        emailAddress = addrValue.value;
        name = nameValue.value;

        // if we want to include short/long toggle views and we have a long view, always add it.
        // if we aren't including a short/long view OR if we are and we haven't parsed enough
        // addresses to reach the cutoff valve yet then add it to the default (short) div.
        if (includeShortLongToggle && optionalLongDiv)
        {
          InsertEmailAddressUnderEnclosingBox(parentBox, optionalLongDiv, true, emailAddress, fullAddress, name);
        }
        if (!includeShortLongToggle || (numAddressesParsed < gNumAddressesToShow))
        {
          InsertEmailAddressUnderEnclosingBox(parentBox, defaultParentDiv, includeShortLongToggle, emailAddress, fullAddress, name);
        }
        
        numAddressesParsed++;
      } 
    } // if enumerator

    if (includeShortLongToggle && (numAddressesParsed > gNumAddressesToShow) && optionalToggleButton)
    {
      optionalToggleButton.removeAttribute('collapsed');
    }
  } // if msgheader parser
}

/* InsertEmailAddressUnderEnclosingBox --> right now all email addresses are borderless titled buttons
   with formatting to make them look like html anchors. When you click on the button,
   you are prompted with a popup asking you what you want to do with the email address

   parentBox --> the enclosing box for all the email addresses for this header. This is needed
                 to control visibility of the header.
   parentDiv --> the DIV the email addresses need to be inserted into.
   includesToggleButton --> true if the parentDiv includes a toggle button we want contnet inserted BEFORE
                            false if the parentDiv does not contain a toggle button. This is required in order
                            to properly detect if we should be inserting addresses before this node....
*/
   
function InsertEmailAddressUnderEnclosingBox(parentBox, parentDiv, includesToggleButton, emailAddress, fullAddress, displayName) 
{
  if ( parentBox ) 
  { 
    var item = document.createElement("mail-emailaddress");
    var itemInDocument;
    if ( item && parentDiv) 
    { 
      if (parentDiv.childNodes.length)
      {
        var child = parentDiv.childNodes[parentDiv.childNodes.length - 1]; 
        if (parentDiv.childNodes.length > 1 || (!includesToggleButton && parentDiv.childNodes.length >= 1) )
        {
          var textNode = document.createElement("text");
          textNode.setAttribute("value", ", ");
          textNode.setAttribute("class", "emailSeparator");
          if (includesToggleButton && numOfEmailsInEnumerator < gNumOfEmailsToShowToggleButtonInFront)
            parentDiv.insertBefore(textNode, child);
          else
            parentDiv.appendChild(textNode);
        }
        if (includesToggleButton && numOfEmailsInEnumerator < gNumOfEmailsToShowToggleButtonInFront)
          itemInDocument = parentDiv.insertBefore(item, child);
        else
          itemInDocument = parentDiv.appendChild(item);
      }
      else
      {
        itemInDocument = parentDiv.appendChild(item);
      }

      itemInDocument.setAttribute("value", fullAddress);    
      itemInDocument.setTextAttribute("emailAddress", emailAddress);
      itemInDocument.setTextAttribute("fullAddress", fullAddress);  
      itemInDocument.setTextAttribute("displayName", displayName);  
      
      if (this.AddExtraAddressProcessing != undefined)
        AddExtraAddressProcessing(emailAddress, itemInDocument);

      hdrViewSetVisible(parentBox, true);
    } 
  } 
}


function UpdateMessageHeaders()
{
  if (gViewAllHeaders)
  {
    fillBoxWithAllHeaders(msgPaneData.ViewAllHeadersBox, false);
    return;
  }

  if (currentHeaderData["subject"])
     hdrViewSetNodeWithBox(msgPaneData.SubjectBox, msgPaneData.SubjectValue, currentHeaderData["subject"].headerValue);
  else
     hdrViewSetNodeWithBox(msgPaneData.SubjectBox, msgPaneData.SubjectValue, "");

  if (currentHeaderData["from"])
    OutputEmailAddresses(msgPaneData.FromBox, msgPaneData.FromValue, currentHeaderData["from"].headerValue, false, "", "", "from"); 
  else
    OutputEmailAddresses(msgPaneData.FromBox, msgPaneData.FromValue, "", false, "", "", ""); 

  if (currentHeaderData["date"])
    hdrViewSetNodeWithBox(msgPaneData.DateBox, msgPaneData.DateValue, currentHeaderData["date"].headerValue); 
  else
    hdrViewSetNodeWithBox(msgPaneData.DateBox, msgPaneData.DateValue, ""); 
  
  if (currentHeaderData["to"])
    OutputEmailAddresses(msgPaneData.ToBox, msgPaneData.ToValueShort, currentHeaderData["to"].headerValue, true, msgPaneData.ToValueLong, msgPaneData.ToValueToggleIcon, "to");
  else
    OutputEmailAddresses(msgPaneData.ToBox, msgPaneData.ToValueShort, "", true, msgPaneData.ToValueLong, msgPaneData.ToValueToggleIcon, "");

  if (currentHeaderData["cc"])
    OutputEmailAddresses(msgPaneData.CcBox, msgPaneData.CcValueShort, currentHeaderData["cc"].headerValue, true, msgPaneData.CcValueLong, msgPaneData.CcValueToggleIcon, "cc");
  else
    OutputEmailAddresses(msgPaneData.CcBox, msgPaneData.CcValueShort, "", true, msgPaneData.CcValueLong, msgPaneData.CcValueToggleIcon, "");
  
  if (currentHeaderData["newsgroups"])
    OutputNewsgroups(msgPaneData.NewsgroupBox, msgPaneData.NewsgroupValue, currentHeaderData, "newsgroups");
  else
    hdrViewSetNodeWithBox(msgPaneData.NewsgroupBox, msgPaneData.NewsgroupValue, ""); 

  if (currentHeaderData["followup-to"])
    OutputNewsgroups(msgPaneData.FollowupToBox, msgPaneData.FollowupToValue, currentHeaderData, "followup-to");
  else
    hdrViewSetNodeWithBox(msgPaneData.FollowupToBox, msgPaneData.NewsgroupValue, ""); 

  if (gShowUserAgent)
  { 
    if (currentHeaderData["user-agent"])
    {
      hdrViewSetNodeWithBox(msgPaneData.UserAgentBox, msgPaneData.UserAgentValue, currentHeaderData["user-agent"].headerValue);
      if (msgPaneData.UserAgentToolbar)
        msgPaneData.UserAgentToolbar.removeAttribute("collapsed");
    }
    else
    {
      hdrViewSetNodeWithBox(msgPaneData.UserAgentBox, msgPaneData.UserAgentValue, "");
      if (msgPaneData.UserAgentToolbar)
        msgPaneData.UserAgentToolbar.setAttribute("collapsed", "true"); 
    }

  }
  
  if (this.FinishEmailProcessing != undefined)
    FinishEmailProcessing();
}

function ClearCurrentHeaders()
{
  currentHeaderData = {};
}

function ShowMessageHeaderPane()
{ 
  if (gViewAllHeaders)
  {
    HideMessageHeaderPane();
    msgPaneData.ViewAllHeadersToolbar.removeAttribute("collapsed");
    msgPaneData.ViewAllHeadersBox.removeAttribute("collapsed");
  }
  else
  {
    msgPaneData.ViewAllHeadersToolbar.setAttribute("collapsed", "true");
    msgPaneData.ViewAllHeadersBox.setAttribute("collapsed", "true");

	/* workaround for 39655 */
    var messagePaneBox = document.getElementById("messagepanebox");
    if (messagePaneBox && gFolderJustSwitched)
      messagePaneBox.setAttribute("collapsed", "true");
          
    var node = document.getElementById("headerPart1");
    if (node)
      node.removeAttribute("collapsed");
    node = document.getElementById("headerPart2");
    if (node)
      node.removeAttribute("collapsed");

	/* more workaround for 39655 */
    if (messagePaneBox && gFolderJustSwitched) {
      messagePaneBox.setAttribute("collapsed", "false");
	  gFolderJustSwitched = false;
    }

/*
    node = document.getElementById("headerPart3");
    if (node)
      node.removeAttribute("collapsed");
*/
  }
}

function HideMessageHeaderPane()
{
  msgPaneData.ViewAllHeadersToolbar.setAttribute("collapsed", "true");
  msgPaneData.ViewAllHeadersBox.setAttribute("collapsed", "true");

  var node = document.getElementById("headerPart1");
  if (node)
    node.setAttribute("collapsed", "true");
  node = document.getElementById("headerPart2");
  if (node)
    node.setAttribute("collapsed", "true");
  node = document.getElementById("headerPart3");
  if (msgPaneData.UserAgentToolbar)
    msgPaneData.UserAgentToolbar.setAttribute("collapsed", "true");

}

// ToggleLongShortAddresses is used to toggle between showing
// all of the addresses on a given header line vs. only the first 'n'
// where 'n' is a user controlled preference. By toggling on the more/less
// images in the header window, we'll hide / show the appropriate div for that header.

function ToggleLongShortAddresses(shortDivID, longDivID)
{
  var shortNode = document.getElementById(shortDivID);
  var longNode = document.getElementById(longDivID);
  var nodeToReset;

  // test to see which if short is already hidden...
  if (shortNode.getAttribute("collapsed") == "true")
  {
     
     hdrViewSetVisible(longNode, false);
     hdrViewSetVisible(shortNode, true);
     nodeToReset = shortNode;
  }
  else
  {
     hdrViewSetVisible(shortNode, false);
     hdrViewSetVisible(longNode, true);
     nodeToReset = longNode;
  }
   
   // HACK ALERT: this is required because of a bug in boxes when you add content
   // to a box which is collapsed, when you then uncollapse that box, the content isn't 
   // displayed. So I'm forcing us to reset the value for each node...
   var endPos = nodeToReset.childNodes.length;
   var index = 0;
   var tempValue;
   while (index < endPos - 1)
   {
     tempValue = nodeToReset.childNodes[index].getAttribute("value");
     nodeToReset.childNodes[index].setAttribute("value", "");
     nodeToReset.childNodes[index].setAttribute("value", tempValue);
     index++;
   }
}

// given a box, iterate through all of the headers and add them to 
// the enclosing box. 
function fillBoxWithAllHeaders(containerBox, boxPartOfPopup)
{
  // clear out the old popup date if there is any...
  while ( containerBox.childNodes.length ) 
    containerBox.removeChild(containerBox.childNodes[0]); 

  for (header in currentHeaderData)
  {
    var innerBox = document.createElement('box');
    innerBox.setAttribute("class", "headerBox");
    innerBox.setAttribute("orient", "horizontal");
    if (boxPartOfPopup)
        innerBox.setAttribute("autostretch", "never");

    // for each header, create a header value and header name then assign those values...
    var headerValueBox = document.createElement('hbox');
    headerValueBox.setAttribute("class", "headerValueBox");

	  var newHeaderTitle  = document.createElement('text');
    newHeaderTitle.setAttribute("class", "headerdisplayname");
	  newHeaderTitle.setAttribute("value", currentHeaderData[header].headerName + ':');
    headerValueBox.appendChild(newHeaderTitle);

	  innerBox.appendChild(headerValueBox);

    var newHeaderValue = document.createElement('html');
    // make sure we are properly resized...
//    if (boxPartOfPopup)
//        newHeaderValue.setAttribute("width", window.innerWidth*.65);
    newHeaderValue.setAttribute("class", "headerValue");
    if (!boxPartOfPopup)
      newHeaderValue.setAttribute("flex", "1");
    innerBox.appendChild(newHeaderValue);
    containerBox.appendChild(innerBox);
    ProcessHeaderValue(innerBox, newHeaderValue, currentHeaderData[header], boxPartOfPopup);
  }
}

// the on create handler for the view all headers popup...
function fillAllHeadersPopup(node)
{
  // don't bother re-filling the popup if we've already done it for the
  // currently displayed message....
  if (gGeneratedViewAllHeaderInfo == true)
    return true; 

  var containerBox = document.getElementById('allHeadersPopupContainer');

  containerBox.setAttribute("class", "header-part1");
  containerBox.setAttribute("align", "vertical");
  containerBox.setAttribute("flex", "1");

  fillBoxWithAllHeaders(containerBox, true);
  gGeneratedViewAllHeaderInfo = true; // don't try to regenerate this information for the currently displayed message
  return true;
}

// containingBox --> the box containing the header
// containerNode --> the div or box that we want to insert this specific header into
// header --> an entry from currentHeaderData contains .headerName and .headerValue
// boxPartOfPopup --> true if the box is part of a popup (certain functions are disabled in this scenario)
function ProcessHeaderValue(containingBox, containerNode, header, boxPartOfPopup)
{
    // in the simplest case, we'll just create a text node for the header
    // and append it to the container Node. for certain headers, we might want to do 
    // extra processing....
    
    var headerName = header.headerName;
    headerName = headerName.toLowerCase();
    if (!boxPartOfPopup && (headerName == "cc" || headerName == "from" || headerName == "to"))
    {
      OutputEmailAddresses(containingBox, containerNode, header.headerValue, false, "", "", headerName)
      return;
    }
    
    var headerValue = header.headerValue;
    headerValue = headerValue.replace(/\n|\r/g,"");
    var textNode = document.createTextNode(headerValue);
    if (headerName == "subject")
    {
        containerNode.setAttribute("class", "subjectvalue headerValue");
    }

    containerNode.appendChild(textNode);
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
		boxNode.removeAttribute("collapsed");
	else
		boxNode.setAttribute("collapsed", "true");
}

