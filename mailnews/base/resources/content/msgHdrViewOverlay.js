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
// mscott@netscape.com. It's critical that the code in here for displaying
// the message headers for a selected message remain as fast as possible. In particular, 
// right now, we only introduce one reflow per message. i.e. if you click on a message in the thread
// pane, we batch up all the changes for displaying the header pane (to, cc, attachements button, etc.) 
// and we make a single pass to display them. It's critical that we maintain this one reflow per message
// view in the message header pane. 
//
////////////////////////////////////////////////////////////////////////////////////

var msgHeaderParserContractID		   = "@mozilla.org/messenger/headerparser;1";
var abAddressCollectorContractID	 = "@mozilla.org/addressbook/services/addressCollecter;1";

var gViewAllHeaders = false;
var gNumAddressesToShow = 3;
var gShowUserAgent = false;
var gCollectIncoming = false;
var gCollectNewsgroup = false;
var gCollapsedHeaderViewMode = false;
var gBuildAttachmentsForCurrentMsg = false;
var gBuildAttachmentPopupForCurrentMsg = true;
var gBuiltExpandedView = false;
var gBuiltCollapsedView = false;
var gOpenLabel;
var gSaveLabel;

var msgHeaderParser = Components.classes[msgHeaderParserContractID].getService(Components.interfaces.nsIMsgHeaderParser);
var abAddressCollector = Components.classes[abAddressCollectorContractID].getService(Components.interfaces.nsIAbAddressCollecter);

// For every possible "view" in the message pane, you need to define the header names you want to
// see in that view. In addition, include information describing how you want that header field to be
// presented. i.e. if it's an email address field, if you want a toggle inserted on the node in case
// of multiple email addresses, etc. We'll then use this static table to dynamically generate header view entries
// which manipulate the UI. 
// When you add a header to one of these view lists you can specify the following properties:
// name: the name of the header. i.e. "to", "subject". This must be in lower case and the name of the
//       header is used to help dynamically generate ids for objects in the document. (REQUIRED)
// useToggle:      true if the values for this header are multiple email addresses and you want a 
//                 a toggle icon to show a short vs. long list (DEFAULT: false)
// useShortView:   (only works on some fields like From). If the field has a long presentation and a
//                 short presentation we'll use the short one. i.e. if you are showing the From field and you
//                 set this to true, we can show just "John Doe" instead of "John Doe <jdoe@netscape.net>".
//                 (DEFAULT: false)
// 
// outputFunction: this is a method which takes a headerEntry (see the definition below) and a header value
//                 This allows you to provide your own methods for actually determining how the header value
//                 is displayed. (DEFAULT: updateHeaderValue which just sets the header value on the text node)

// Our first view is the collapsed view. This is very light weight view of the data. We only show a couple
// fields.
var gCollapsedHeaderList = [ {name:"subject", outputFunction:updateHeaderValueInTextNode},
                             {name:"from", useShortView:true, outputFunction:OutputEmailAddresses},
                             {name:"date", outputFunction:updateHeaderValueInTextNode}];

// We also have an expanded header view. This shows many of your more common (and useful) headers.
var gExpandedHeaderList = [ {name:"subject"}, 
                            {name:"from", outputFunction:OutputEmailAddresses},
                            {name:"reply-to", isEmailAddress:true, outputFunction:OutputEmailAddresses},
                            {name:"date"},
                            {name:"to", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"cc", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"bcc", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"newsgroups", outputFunction:OutputNewsgroups},
                            {name:"followup-to", outputFunction:OutputNewsgroups} ];

// Now, for each view the message pane can generate, we need a global table of headerEntries. These
// header entry objects are generated dynamically based on the static date in the header lists (see above)
// and elements we find in the DOM based on properties in the header lists. 
var gCollapsedHeaderView = new Array;
var gExpandedHeaderView = new Array;

// currentHeaderData --> this is an array of header name and value pairs for the currently displayed message.
//                       it's purely a data object and has no view information. View information is contained in the view objects.
//                       for a given entry in this array you can ask for:
// .headerName ---> name of the header (i.e. 'to'). Always stored in lower case
// .headerValue --> value of the header "johndoe@netscape.net"
var currentHeaderData = {};

// For the currently displayed message, we store all the attachment data. When displaying a particular
// view, it's up to the view layer to extract this attachment data and turn it into something useful.
// For a given entry in the attachments list, you can ask for the following properties:
// .contentType --> the content type of the attachment
// url --> an imap, or mailbox url which can be used to fetch the message
// uri --> an RDF URI which refers to the message containig the attachment
// notDownloaded --> boolean flag stating whether the attachment is downloaded or not.
var currentAttachments = new Array();

// createHeaderEntry --> our constructor method which creates a header Entry 
// based on an entry in one of the header lists. A header entry is different from a header list.
// a header list just describes how you want a particular header to be presented. The header entry
// actually has knowledge about the DOM and the actual DOM elements associated with the header.
// prefix --> the name of the view (i.e. "collapsed", "expanded")
// headerListInfo --> entry from a header list.
function createHeaderEntry(prefix, headerListInfo)
{
  var partialIDName = prefix + headerListInfo.name;
  this.enclosingBox = document.getElementById(partialIDName + 'Box');
  this.textNode = document.getElementById(partialIDName + 'Value');
  this.isValid = false;

  if ("useToggle" in headerListInfo)
  {
    this.useToggle = headerListInfo.useToggle;
    if (this.useToggle) // find the toggle icon in the document
    {
      this.toggleIcon = this.enclosingBox.toggleIcon;
      this.longTextNode = this.enclosingBox.longEmailAddresses;
      this.textNode = this.enclosingBox.emailAddresses;
    }
  }
  else
   this.useToggle = false;

  if ("useShortView" in headerListInfo)
  {
    this.useShortView = headerListInfo.useShortView;
    this.enclosingBox.emailAddressNode = this.textNode;
  }
  else
    this.useShortView = false;

  if ("outputFunction" in headerListInfo)
    this.outputFunction = headerListInfo.outputFunction;
  else
    this.outputFunction = updateHeaderValue;
}

function initializeHeaderViewTables()
{
  // iterate over each header in our header list arrays and create header entries 
  // for each one. These header entries are then stored in the appropriate header table

  if (!gCollapsedHeaderView.length) // if we haven't already initialized the collapsed view...
    for (var index = 0; index < gCollapsedHeaderList.length; index++)
    {
      gCollapsedHeaderView[gCollapsedHeaderList[index].name] = 
        new createHeaderEntry('collapsed', gCollapsedHeaderList[index]);
    }

  if (!gExpandedHeaderView.length)
  {
    for (index = 0; index < gExpandedHeaderList.length; index++)
    {
      var headerName = gExpandedHeaderList[index].name;
      gExpandedHeaderView[headerName] = new createHeaderEntry('expanded', gExpandedHeaderList[index]);
    }
    
    if (gShowUserAgent)
    {
      var userAgentEntry = {name:"user-agent", outputFunction:updateHeaderValue};
      gExpandedHeaderView[userAgentEntry.name] = new createHeaderEntry('expanded', userAgentEntry);
    }
  } // if we need to initialize the expanded header view...
}

function OnLoadMsgHeaderPane()
{
  // HACK...force our XBL bindings file to be load before we try to create our first xbl widget....
  // otherwise we have problems.

  document.loadBindingDocument('chrome://messenger/content/mailWidgets.xml');
  
  // load any preferences that at are global with regards to 
  // displaying a message...
  gNumAddressesToShow = pref.getIntPref("mailnews.max_header_display_length");
  gCollectIncoming = pref.getBoolPref("mail.collect_email_address_incoming");
  gCollectNewsgroup = pref.getBoolPref("mail.collect_email_address_newsgroup");
  gShowUserAgent = pref.getBoolPref("mailnews.headers.showUserAgent");
  initializeHeaderViewTables();

  var toggleHeaderView = document.getElementById("msgHeaderView");
  var initialCollapsedSetting = toggleHeaderView.getAttribute("state");
  if (initialCollapsedSetting == "true")
    gCollapsedHeaderViewMode = true;   
}

// The messageHeaderSink is the class that gets notified of a message's headers as we display the message
// through our mime converter. 

var messageHeaderSink = {
    onStartHeaders: function()
    {
      // every time we start to redisplay a message, check the view all headers pref....
      var showAllHeadersPref = pref.getIntPref("mail.show_headers");
      if (showAllHeadersPref == 2)
      {
        gViewAllHeaders = true;
      }
      else
      {
        if (gViewAllHeaders) // if we currently are in view all header mode, rebuild our header view so we remove most of the header data
        { 
          hideHeaderView(gExpandedHeaderView);
          gExpandedHeaderView = {};
          initializeHeaderViewTables(); 
        }
                
        gViewAllHeaders = false;
      }

      ClearCurrentHeaders();
      gBuiltExpandedView = false;
      gBuiltCollapsedView = false;
      gBuildAttachmentsForCurrentMsg = false;
      gBuildAttachmentPopupForCurrentMsg = true;
      ClearAttachmentTreeList();
      ClearEditMessageButton();
    },

    onEndHeaders: function() 
    {
      // WARNING: This is the ONLY routine inside of the message Header Sink that should 
      // trigger a reflow!
      CheckNotify();
      
      ClearHeaderView(gCollapsedHeaderView);
      ClearHeaderView(gExpandedHeaderView);

      EnsureSubjectValue(); // make sure there is a subject even if it's empty so we'll show the subject and the twisty
      
      ShowMessageHeaderPane();
      UpdateMessageHeaders();
      if (gIsEditableMsgFolder)
        ShowEditMessageButton();
    },

    processHeaders: function(headerNames, headerValues, numHeaders, dontCollectAddress)
    {
      this.onStartHeaders(); 

      var index = 0; 
      // process each header
      while (index < numHeaders)
      {
        // for consistancy sake, let's force all header names to be lower case so
        // we don't have to worry about looking for: Cc and CC, etc.
        var lowerCaseHeaderName = headerNames[index].toLowerCase();
        
        var foo = new Object;        
        foo.headerValue = headerValues[index];
        foo.headerName = headerNames[index];

        // some times, you can have multiple To or cc lines....in this case, we want to APPEND 
        // these headers into one. 
        if ( (lowerCaseHeaderName == 'to' || lowerCaseHeaderName == 'cc') && ( lowerCaseHeaderName in currentHeaderData))
        {
          currentHeaderData[lowerCaseHeaderName].headerValue = currentHeaderData[lowerCaseHeaderName].headerValue + ',' + foo.headerValue;
        }
        else
         currentHeaderData[lowerCaseHeaderName] = foo;

        if (lowerCaseHeaderName == "from")
        {
          if (foo.headerValue && abAddressCollector && ((gCollectIncoming && !dontCollectAddress) || (gCollectNewsgroup && dontCollectAddress)))
           abAddressCollector.collectUnicodeAddress(foo.headerValue);  
        }
       
        index++;
      }

      this.onEndHeaders();
    },

    handleAttachment: function(contentType, url, displayName, uri, notDownloaded) 
    {
      currentAttachments.push (new createNewAttachmentInfo(contentType, url, displayName, uri, notDownloaded));
      // if we have an attachment, set the MSG_FLAG_ATTACH flag on the hdr
      // this will cause the "message with attachment" icon to show up
      // in the thread pane
      // we only need to do this on the first attachment
      var numAttachments = currentAttachments.length;
      if (numAttachments == 1) {
        try {
          // convert the uri into a hdr
          var hdr = messenger.messageServiceFromURI(uri).messageURIToMsgHdr(uri);
          hdr.markHasAttachments(true);
        }
        catch (ex) {
          dump("ex = " + ex + "\n");
        }
      }
    },
    
    onEndAllAttachments: function()
    {
      // AddSaveAllAttachmentsMenu();
      if (gCollapsedHeaderViewMode)
        displayAttachmentsForCollapsedView();
      else
        displayAttachmentsForExpandedView();
    }
};

function EnsureSubjectValue()
{
  if (!('subject' in currentHeaderData))
  {
    var foo = new Object;
    foo.headerValue = "";
    foo.headerName = 'subject';
    currentHeaderData[foo.headerName] = foo;
  } 
}

function CheckNotify()
{
  if (this.NotifyClearAddresses != undefined)
    NotifyClearAddresses();
}


// flush out any local state being held by a header entry for a given
// table
function ClearHeaderView(headerTable)
{
  for (index in headerTable)
  {
     var headerEntry = headerTable[index];
     if (headerEntry.useToggle)
     {
       headerEntry.enclosingBox.clearEmailAddresses();    
     }

     headerEntry.valid = false;
  }
}

// make sure that any valid header entry in the table is collapsed
function hideHeaderView(headerTable)
{
  for (index in headerTable)
  {
    headerTable[index].enclosingBox.setAttribute('collapsed', true);
  }
}

// make sure that any valid header entry in the table specified is 
// visible
function showHeaderView(headerTable)
{
  var headerEntry;
  for (index in headerTable)
  {
    headerEntry = headerTable[index];
    if (headerEntry.valid)
    {
      headerEntry.enclosingBox.removeAttribute('collapsed');
    }
    else // if the entry is invalid, always make sure it's collapsed
      headerEntry.enclosingBox.setAttribute('collapsed', true);
  }
}

// make sure the appropriate fields within the currently displayed view header mode
// are collapsed or visible...
function updateHeaderViews()
{
  if (gCollapsedHeaderViewMode)
  {
    showHeaderView(gCollapsedHeaderView);
    displayAttachmentsForCollapsedView();
  }
  else
  {
    showHeaderView(gExpandedHeaderView);
    displayAttachmentsForExpandedView();
  }
}

function ToggleHeaderView ()
{
  var expandedNode = document.getElementById("expandedHeaderView");
  var collapsedNode = document.getElementById("collapsedHeaderView");
  var toggleHeaderView = document.getElementById("msgHeaderView");

  if (gCollapsedHeaderViewMode)
  {          
    gCollapsedHeaderViewMode = false;
    // hide the current view
    hideHeaderView(gCollapsedHeaderView);
    // update the current view
    UpdateMessageHeaders();
    
    // now uncollapse / collapse the right views
    expandedNode.removeAttribute("collapsed");
    collapsedNode.setAttribute("collapsed", "true");
  }
  else
  {
    gCollapsedHeaderViewMode = true;
    // hide the current view
    hideHeaderView(gExpandedHeaderView);
    // update the current view
    UpdateMessageHeaders();
    
    // now uncollapse / collapse the right views
    collapsedNode.removeAttribute("collapsed");
    expandedNode.setAttribute("collapsed", "true");
  }  

  if (gCollapsedHeaderViewMode)
    toggleHeaderView.setAttribute("state", "true");
  else
    toggleHeaderView.setAttribute("state", "false");
}

// default method for updating a header value into a header entry
function updateHeaderValue(headerEntry, headerValue)
{
  headerEntry.enclosingBox.headerValue = headerValue;
}

function updateHeaderValueInTextNode(headerEntry, headerValue)
{
  headerEntry.textNode.setAttribute("value", headerValue);
}

function createNewHeaderView(headerName)
{
  var idName = 'expanded' + headerName + 'Box';
  var newHeader = document.createElement("mail-headerfield");
  newHeader.setAttribute('id', idName);
  newHeader.setAttribute('label', currentHeaderData[headerName].headerName + ':');
  newHeader.setAttribute('collapsed', true);


  // this new element needs to be inserted into the view...
  var topViewNode = document.getElementById('expandedHeaders');

  topViewNode.appendChild(newHeader);
  
  this.enclosingBox = newHeader
  this.isValid = false;
  this.useToggle = false;
  this.useShortView = false;
  this.outputFunction = updateHeaderValue;
}

// UpdateMessageHeaders: Iterate through all the current header data we received from mime for this message
// for each header entry table, see if we have a corresponding entry for that header. i.e. does the particular
// view care about this header value. if it does then call updateHeaderEntry
function UpdateMessageHeaders()
{
  // iterate over each header we received and see if we have a matching entry in each
  // header view table...

  for (headerName in currentHeaderData)
  {
    var headerField = currentHeaderData[headerName];
    var headerEntry;

    if (gCollapsedHeaderViewMode && !gBuiltCollapsedView)
    { 
      headerEntry = gCollapsedHeaderView[headerName];
      if (headerEntry != undefined && headerEntry)
      {  
        headerEntry.outputFunction(headerEntry, headerField.headerValue);
        headerEntry.valid = true;    
      }
    }
    else if (!gCollapsedHeaderViewMode && !gBuiltExpandedView)
    {
      headerEntry = gExpandedHeaderView[headerName];
      if (headerEntry == undefined && gViewAllHeaders)
      {
        // for view all headers, if we don't have a header field for this value....cheat and create one....then
        // fill in a headerEntry
        gExpandedHeaderView[headerName] = new createNewHeaderView(headerName);
        headerEntry = gExpandedHeaderView[headerName];
      }

      if (headerEntry != undefined && headerEntry)
      {
        headerEntry.outputFunction(headerEntry, headerField.headerValue);
        headerEntry.valid = true;
      }
    } // if we are in expanded view....
  }

  if (gCollapsedHeaderViewMode)
   gBuiltCollapsedView = true;
  else
   gBuiltExpandedView = true;

  // now update the view to make sure the right elements are visible
  updateHeaderViews();
  
  if (this.FinishEmailProcessing != undefined)
    FinishEmailProcessing();
}

function ClearCurrentHeaders()
{
  currentHeaderData = {};
  currentAttachments = new Array();
}

function ShowMessageHeaderPane()
{ 
  var node;
  if (gCollapsedHeaderViewMode)
  {          
    node = document.getElementById("collapsedHeaderView");
    if (node)
      node.removeAttribute("collapsed");
  }
  else
  {
    node = document.getElementById("expandedHeaderView");
    if (node)
      node.removeAttribute("collapsed");
  }

	/* workaround for 39655 */
  if (gFolderJustSwitched) 
  {
    var el = document.getElementById("msgHeaderView");
    el.setAttribute("style", el.getAttribute("style"));
    gFolderJustSwitched = false;    
  }
}

function HideMessageHeaderPane()
{
  var node = document.getElementById("collapsedHeaderView");
  if (node)
    node.setAttribute("collapsed", "true");

  node = document.getElementById("expandedHeaderView");
  if (node)
    node.setAttribute("collapsed", "true");
}

function OutputNewsgroups(headerEntry, headerValue)
{ 
  headerValue = headerValue.replace(/,/g,", ");
  updateHeaderValue(headerEntry, headerValue);
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
// useShortView --> if true, we'll only generate the Name of the email address field instead of
//                        showing the name + the email address.

function OutputEmailAddresses(headerEntry, emailAddresses)
{
  if ( !emailAddresses ) return;

  if (msgHeaderParser)
  {
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
        if (headerEntry.useToggle)
        {
          var addresses = {};
          addresses.emailAddress = emailAddress;
          addresses.fullAddress = fullAddress;
          addresses.displayName = name;

          headerEntry.enclosingBox.addAddressView(addresses);
        }
        else
        {
          updateEmailAddressNode(headerEntry.enclosingBox.emailAddressNode, emailAddress, fullAddress, name, headerEntry.useShortView);
        }
        
        numAddressesParsed++;
      } 
    } // if enumerator
    
    if (headerEntry.useToggle)
      headerEntry.enclosingBox.buildViews(gNumAddressesToShow);
  } // if msgheader parser
}

function updateEmailAddressNode(emailAddressNode, emailAddress, fullAddress, displayName, useShortView)
{
  if (useShortView && displayName)
    emailAddressNode.setAttribute("label", displayName);  
  else
    emailAddressNode.setAttribute("label", fullAddress);    
  emailAddressNode.setTextAttribute("emailAddress", emailAddress);
  emailAddressNode.setTextAttribute("fullAddress", fullAddress);  
  emailAddressNode.setTextAttribute("displayName", displayName);  
  
  if (this.AddExtraAddressProcessing != undefined)
    AddExtraAddressProcessing(emailAddress, emailAddressNode);
}

function AddNodeToAddressBook (emailAddressNode)
{
  if (emailAddressNode)
  {
    var primaryEmail = emailAddressNode.getAttribute("emailAddress");
    var displayName = emailAddressNode.getAttribute("displayName");
    window.openDialog("chrome://messenger/content/addressbook/abNewCardDialog.xul",
                      "",
                      "chrome,titlebar,resizable=no", 
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

// CopyEmailAddress takes the email address title button, extracts
// the email address we stored in there and copies it to the clipboard
function CopyEmailAddress(emailAddressNode)
{
  if (emailAddressNode)
  {
    var emailAddress = emailAddressNode.getAttribute("emailAddress");

    var contractid = "@mozilla.org/widget/clipboardhelper;1";
    var iid = Components.interfaces.nsIClipboardHelper;
    var clipboard = Components.classes[contractid].getService(iid);
    clipboard.copyString(emailAddress);
  }
}

// CreateFilter opens the Message Filters and Filter Rules dialogs.
//The Filter Rules dialog has focus. The window is prefilled with filtername <email address>
//Sender condition is selected and the value is prefilled <email address>
function CreateFilter(emailAddressNode)
{
  if (emailAddressNode)
  {
     var emailAddress = emailAddressNode.getAttribute("emailAddress");
     if (emailAddress){
         top.MsgFilters(emailAddress);
     }
  }
}

// createnewAttachmentInfo --> constructor method for creating new attachment object which goes into the
// data attachment array.
function createNewAttachmentInfo(contentType, url, displayName, uri, notDownloaded)
{
  this.contentType = contentType;
  this.url = url;
  this.displayName = displayName;
  this.uri = uri;
  this.notDownloaded = notDownloaded;
}

function saveAttachment(contentType, url, displayName, messageUri)
{
  messenger.saveAttachment(contentType, url, displayName, messageUri);
}

function openAttachment(contentType, url, displayName, messageUri)
{
  messenger.openAttachment(contentType, url, displayName, messageUri);
}

function printAttachmentAttachment(contentType, url, displayName, messageUri)
{
  // we haven't implemented the ability to print attachments yet...
  // messenger.printAttachment(contentType, url, displayName, messageUri);
}

// this is our onclick handler for the attachment tree. 
// A double click in a tree cell simulates "opening" the attachment....
function attachmentTreeClick(event)
{ 
    // we only care about button 0 (left click) events
    if (event.button != 0) return;

    if (event.detail == 2) // double click
    {
        var target = event.originalTarget;
        var item = target.parentNode.parentNode;
		if (item.localName == "treeitem")
    {
			var commandStringSuffix = item.getAttribute("commandSuffix");
      var openString = 'openAttachment' + commandStringSuffix;
      eval(openString);
    }
    }
}

// on command handlers for the attachment tree context menu...
// commandPrefix matches one of our existing functions (openAttachment, saveAttachment, etc.) which we'll add to the command suffix
// found on the tree item....
function handleAttachmentSelection(commandPrefix)
{
  // get the selected attachment...and call openAttachment on it...
  var attachmentTree = document.getElementById('attachmentTree');
  var selectedAttachments = attachmentTree.selectedItems;
  var treeItem = selectedAttachments[0];
  var commandStringSuffix = treeItem.getAttribute("commandSuffix");
  var openString = commandPrefix + commandStringSuffix;
  eval(openString);
}

function generateCommandSuffixForAttachment(attachment)
{
  return "('" + attachment.contentType + "', '" + attachment.url + "', '" + escape(attachment.displayName) + "', '" + attachment.uri + "')";
}

function displayAttachmentsForExpandedView()
{
  var numAttachments = currentAttachments.length;
  if (numAttachments > 0 && !gBuildAttachmentsForCurrentMsg)
  {
    var attachmentList = document.getElementById('attachmentsBody');
    var row, cell, item;
    for (index in currentAttachments)
    {
      var attachment = currentAttachments[index];
      // we need to create a tree item, a tree row and a tree cell to insert the attachment
      // into the attachment tree..

	  item = document.createElement("treeitem");
	  row = document.createElement("treerow");
	  cell = document.createElement("treecell");

      cell.setAttribute('class', "treecell-iconic"); 
      cell.setAttribute("label", attachment.displayName);
      cell.setAttribute("tooltip", "attachmentTreeTooltip");
      item.setAttribute("commandSuffix", generateCommandSuffixForAttachment(attachment)); // set the command suffix on the tree item...
      setApplicationIconForAttachment(attachment, cell);
	  row.appendChild(cell);
	  item.appendChild(row);
	  attachmentList.appendChild(item);
    } // for each attachment
    gBuildAttachmentsForCurrentMsg = true;
  }

  var attachmentNode = document.getElementById('expandedAttachmentBox');
  if (numAttachments > 0)  // make sure the attachment button is visible
    attachmentNode.removeAttribute('collapsed');
  else
    attachmentNode.setAttribute('collapsed', true);
}

// attachment --> the attachment struct containing all the information on the attachment
// treeCell --> the tree cell currently showing the attachment.
function setApplicationIconForAttachment(attachment, treeCell)
{
   // generate a moz-icon url for the attachment so we'll show a nice icon next to it.
   treeCell.setAttribute('src', "moz-icon:" + "//" + attachment.displayName + "?size=16&contentType=" + attachment.contentType);
}

function displayAttachmentsForCollapsedView()
{
  var numAttachments = currentAttachments.length;
  var attachmentNode = document.getElementById('collapsedAttachmentBox');
  if (numAttachments > 0)  // make sure the attachment button is visible
  {
    attachmentNode.removeAttribute('hide');
  }
  else
  {
    attachmentNode.setAttribute('hide', true);
  }
}

// Public method called to generate a tooltip over an attachment
function FillInAttachmentTooltip(cellNode)
{
  var attachmentName = cellNode.getAttribute("label");
  var textNode = document.getElementById("attachmentTreeTooltipText");
  textNode.setAttribute('value', attachmentName);
  return true;
}

// Public method called when we create the attachments file menu
function FillAttachmentListPopup(popup)
{
  // the FE sometimes call this routie TWICE...I haven't been able to figure out why yet...
  // protect against it...

  if (!gBuildAttachmentPopupForCurrentMsg) return; 

  // otherwise we need to build the attachment view...
  // First clear out the old view...
  ClearAttachmentMenu(popup);


  for (index in currentAttachments)
  {
    addAttachmentToPopup(popup, currentAttachments[index]);
  }

  gBuildAttachmentPopupForCurrentMsg = false;

}

// Public method used to clear the file attachment menu
function ClearAttachmentMenu(popup) 
{ 
  if ( popup ) 
  { 
     while ( popup.childNodes.length > 2 ) 
       popup.removeChild(popup.childNodes[0]); 
  } 
}

// Public method used to determine the number of attachments for the currently displayed message...
function GetNumberOfAttachmentsForDisplayedMessage()
{
  return currentAttachments.length;
}

// private method used to build up a menu list of attachments
function addAttachmentToPopup(popup, attachment) 
{ 
  if (popup)
  { 
    var item = document.createElement('menu');
    if ( item ) 
    {     
      // insert the item just before the separator...the separator is the 2nd to last element in the popup.
      item.setAttribute('class', 'menu-iconic');
      setApplicationIconForAttachment(attachment,item);
      var numItemsInPopup = popup.childNodes.length;
      item = popup.insertBefore(item, popup.childNodes[numItemsInPopup-2]);
      item.setAttribute('label', attachment.displayName); 
      var oncommandPrefix = generateCommandSuffixForAttachment(attachment);

      var openpopup = document.createElement('menupopup');
      openpopup = item.appendChild(openpopup);

      var menuitementry = document.createElement('menuitem');     
      menuitementry.setAttribute('oncommand', 'openAttachment' + oncommandPrefix); 

      if (!gSaveLabel || !gOpenLabel) {
        var messengerBundle = document.getElementById("bundle_messenger");
        gSaveLabel = messengerBundle.getString("saveLabel");
        gOpenLabel = messengerBundle.getString("openLabel");
      }

      menuitementry.setAttribute('label', gOpenLabel); 
      menuitementry = openpopup.appendChild(menuitementry);

      var menuseparator = document.createElement('menuseparator');
      openpopup.appendChild(menuseparator);
      
      menuitementry = document.createElement('menuitem');
      menuitementry.setAttribute('oncommand', 'saveAttachment' + oncommandPrefix); 
      menuitementry.setAttribute('label', gSaveLabel); 
      menuitementry = openpopup.appendChild(menuitementry);
    }  // if we created a menu item for this attachment...
  } // if we have a popup
} 

function SaveAllAttachments()
{
 try 
 {
   // convert our attachment data into some c++ friendly structs
   var attachmentContentTypeArray = new Array();
   var attachmentUrlArray = new Array();
   var attachmentDisplayNameArray = new Array();
   var attachmentMessageUriArray = new Array();

   // populate these arrays..
   for (index in currentAttachments)
   {
     var attachment = currentAttachments[index];
     attachmentContentTypeArray[index] = attachment.contentType;
     attachmentUrlArray[index] = attachment.url;
     attachmentDisplayNameArray[index] = escape(attachment.displayName);
     attachmentMessageUriArray[index] = attachment.uri;
   }

   // okay the list has been built...now call our save all attachments code...
   messenger.saveAllAttachments(attachmentContentTypeArray.length,
                                attachmentContentTypeArray, attachmentUrlArray,
                                attachmentDisplayNameArray, attachmentMessageUriArray);
 }
 catch (ex)
 {
   dump ("** failed to save all attachments **\n");
 }
}

function ClearAttachmentTreeList() 
{ 
  var attachmentTreebody = document.getElementById("attachmentsBody"); 
  if ( attachmentTreebody ) 
  { 
     while ( attachmentTreebody.childNodes.length ) 
       attachmentTreebody.removeChild(attachmentTreebody.childNodes[0]); 
  } 
}

function ShowEditMessageButton() 
{
  var editBox = document.getElementById("editMessageBox");
  if (editBox)
    editBox.removeAttribute("collapsed");
} 

function ClearEditMessageButton() 
{ 
  var editBox = document.getElementById("editMessageBox");
  if (editBox)
    editBox.setAttribute("collapsed", "true");
}

