/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This is where functions related to displaying the headers for a selected message in the
   message pane live. */

////////////////////////////////////////////////////////////////////////////////////
// Warning: if you go to modify any of these JS routines please get a code review from
// mscott@netscape.com. It's critical that the code in here for displaying
// the message headers for a selected message remain as fast as possible. In particular, 
// right now, we only introduce one reflow per message. i.e. if you click on a message in the thread
// pane, we batch up all the changes for displaying the header pane (to, cc, attachements button, etc.) 
// and we make a single pass to display them. It's critical that we maintain this one reflow per message
// view in the message header pane. 
////////////////////////////////////////////////////////////////////////////////////

var msgHeaderParserContractID		   = "@mozilla.org/messenger/headerparser;1";
var abAddressCollectorContractID	 = "@mozilla.org/addressbook/services/addressCollecter;1";

var gViewAllHeaders = false;
var gNumAddressesToShow = 3;
var gShowOrganization = false;
var gShowUserAgent = false;
var gCollectIncoming = false;
var gCollectOutgoing = false;
var gCollectNewsgroup = false;
var gCollapsedHeaderViewMode = false;
var gCollectAddressTimer = null;
var gCollectAddress = null;
var gBuildAttachmentsForCurrentMsg = false;
var gBuildAttachmentPopupForCurrentMsg = true;
var gBuiltExpandedView = false;
var gBuiltCollapsedView = false;
var gOpenLabel;
var gOpenLabelAccesskey;
var gSaveLabel;
var gSaveLabelAccesskey;
var gMessengerBundle;
var gProfileDirURL;
var gIOService;
var gFileHandler;

var msgHeaderParser = Components.classes[msgHeaderParserContractID].getService(Components.interfaces.nsIMsgHeaderParser);
var abAddressCollector = Components.classes[abAddressCollectorContractID].getService(Components.interfaces.nsIAbAddressCollecter);

// other components may listen to on start header & on end header notifications for each message we display
// to do that you need to add yourself to our gMessageListeners array with object that has two properties:
// onStartHeaders and onEndHeaders.
var gMessageListeners = new Array;

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
                            {name:"reply-to", outputFunction:OutputEmailAddresses},
                            {name:"date"},
                            {name:"to", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"cc", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"bcc", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"newsgroups", outputFunction:OutputNewsgroups},
                            {name:"followup-to", outputFunction:OutputNewsgroups} ];

// Now, for each view the message pane can generate, we need a global table of headerEntries. These
// header entry objects are generated dynamically based on the static date in the header lists (see above)
// and elements we find in the DOM based on properties in the header lists. 
var gCollapsedHeaderView = {};
var gExpandedHeaderView  = {};

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
  var index;
  for (index = 0; index < gCollapsedHeaderList.length; index++)
  {
    gCollapsedHeaderView[gCollapsedHeaderList[index].name] = 
      new createHeaderEntry('collapsed', gCollapsedHeaderList[index]);
  }

  for (index = 0; index < gExpandedHeaderList.length; index++)
  {
    var headerName = gExpandedHeaderList[index].name;
    gExpandedHeaderView[headerName] = new createHeaderEntry('expanded', gExpandedHeaderList[index]);
  }
  
  if (gShowOrganization)
  {
    var organizationEntry = {name:"organization", outputFunction:updateHeaderValue};
    gExpandedHeaderView[organizationEntry.name] = new createHeaderEntry('expanded', organizationEntry);
  }
  
  if (gShowUserAgent)
  {
    var userAgentEntry = {name:"user-agent", outputFunction:updateHeaderValue};
    gExpandedHeaderView[userAgentEntry.name] = new createHeaderEntry('expanded', userAgentEntry);
  }
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
  gCollectOutgoing = pref.getBoolPref("mail.collect_email_address_outgoing");
  gShowUserAgent = pref.getBoolPref("mailnews.headers.showUserAgent");
  gShowOrganization = pref.getBoolPref("mailnews.headers.showOrganization");
  initializeHeaderViewTables();

  var toggleHeaderView = document.getElementById("msgHeaderView");
  var initialCollapsedSetting = toggleHeaderView.getAttribute("state");
  if (initialCollapsedSetting == "true")
    gCollapsedHeaderViewMode = true;   

  // dispatch an event letting any listeners know that we have loaded the message pane
  var event = document.createEvent('Events');
  event.initEvent('messagepane-loaded', false, true);
  var headerViewElement = document.getElementById("msgHeaderView");
  headerViewElement.dispatchEvent(event);
}

function OnUnloadMsgHeaderPane()
{
  // dispatch an event letting any listeners know that we have unloaded the message pane
  var event = document.createEvent('Events');
  event.initEvent('messagepane-unloaded', false, true);
  var headerViewElement = document.getElementById("msgHeaderView");
  headerViewElement.dispatchEvent(event);
}

// The messageHeaderSink is the class that gets notified of a message's headers as we display the message
// through our mime converter. 

var messageHeaderSink = {
    onStartHeaders: function()
    {

      // clear out any pending collected address timers...
      if (gCollectAddressTimer)
      {
        gCollectAddress = "";        
        clearTimeout(gCollectAddressTimer);
        gCollectAddressTimer = null;
      }

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
      ClearAttachmentList();
      ClearEditMessageButton();

      for (index in gMessageListeners)
        gMessageListeners[index].onStartHeaders();
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

    processHeaders: function(headerNameEnumerator, headerValueEnumerator, dontCollectAddress)
    {
      this.onStartHeaders(); 

      while (headerNameEnumerator.hasMore()) 
      {
        var header = new Object;        
        header.headerValue = headerValueEnumerator.getNext();
        header.headerName = headerNameEnumerator.getNext();

        // for consistancy sake, let's force all header names to be lower case so
        // we don't have to worry about looking for: Cc and CC, etc.
        var lowerCaseHeaderName = header.headerName.toLowerCase();

        // if we have an x-mailer string, put it in the user-agent slot which we know how to handle
        // already. 
        if (lowerCaseHeaderName == "x-mailer")
          lowerCaseHeaderName = "user-agent";          
        
        // according to RFC 2822, certain headers
        // can occur "unlimited" times
        if (lowerCaseHeaderName in currentHeaderData)
        {
          // sometimes, you can have multiple To or Cc lines....
          // in this case, we want to append these headers into one.
          if (lowerCaseHeaderName == 'to' || lowerCaseHeaderName == 'cc')
            currentHeaderData[lowerCaseHeaderName].headerValue = currentHeaderData[lowerCaseHeaderName].headerValue + ',' + header.headerValue;
          else 
          {  
            // use the index to create a unique header name like:
            // received5, received6, etc
            currentHeaderData[lowerCaseHeaderName + index] = header;
          }
        }
        else
         currentHeaderData[lowerCaseHeaderName] = header;

        if (lowerCaseHeaderName == "from")
        {
          if (header.value && abAddressCollector) 
          {
            if ((gCollectIncoming && !dontCollectAddress) || 
                (gCollectNewsgroup && dontCollectAddress))
            {
              gCollectAddress = header.headerValue;
              // collect, and add card if doesn't exist, unknown preferred send format
              gCollectAddressTimer = setTimeout('abAddressCollector.collectUnicodeAddress(gCollectAddress, true, Components.interfaces.nsIAbPreferMailFormat.unknown);', 2000);
            }
            else if (gCollectOutgoing) 
            {
              // collect, but only update existing cards, unknown preferred send format
              gCollectAddress = header.headerValue;
              gCollectAddressTimer = setTimeout('abAddressCollector.collectUnicodeAddress(gCollectAddress, false, Components.interfaces.nsIAbPreferMailFormat.unknown);', 2000);
            }
          }
        } // if lowerCaseHeaderName == "from"
      } // while we have more headers to parse

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
        // we also have to enable the File/Attachments menuitem
        var node = document.getElementById("fileAttachmentMenu");
        if (node)
          node.removeAttribute("disabled");

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
    },

    onEndMsgDownload: function(url)
    {
      OnMsgLoaded(url);
    },

    mSecurityInfo  : null,
    getSecurityInfo: function()
    {
      return this.mSecurityInfo;
    },
    setSecurityInfo: function(aSecurityInfo)
    {
      this.mSecurityInfo = aSecurityInfo;
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
  if ("NotifyClearAddresses" in this)
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
    headerTable[index].enclosingBox.collapsed = true;
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
      headerEntry.enclosingBox.collapsed = false;
    }
    else // if the entry is invalid, always make sure it's collapsed
      headerEntry.enclosingBox.collapsed = true;
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
    expandedNode.collapsed = false;
    collapsedNode.collapsed = true;
  }
  else
  {
    gCollapsedHeaderViewMode = true;
    // hide the current view
    hideHeaderView(gExpandedHeaderView);
    // update the current view
    UpdateMessageHeaders();
    
    // now uncollapse / collapse the right views
    collapsedNode.collapsed = false;
    expandedNode.collapsed = true;
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
  headerEntry.textNode.value = headerValue;
}

function createNewHeaderView(headerName)
{
  var idName = 'expanded' + headerName + 'Box';
  var newHeader = document.createElement("mail-headerfield");
  newHeader.setAttribute('id', idName);
  newHeader.setAttribute('label', currentHeaderData[headerName].headerName + ':');
  // all mail-headerfield elements are keyword related
  newHeader.setAttribute('keywordrelated','true');
  newHeader.collapsed = true;

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
    var headerEntry = null;

    if (headerName == "subject")
    {
      try {
        if (gDBView.keyForFirstSelectedMessage == nsMsgKey_None)
        {
          var folder = null;
          if (gCurrentFolderUri)
            folder = RDF.GetResource(gCurrentFolderUri).QueryInterface(Components.interfaces.nsIMsgFolder);
          setTitleFromFolder(folder, headerField.headerValue);
        }
      } catch (ex) {}
    }
    
    if (gCollapsedHeaderViewMode && !gBuiltCollapsedView)
    { 
      if (headerName in gCollapsedHeaderView)
        headerEntry = gCollapsedHeaderView[headerName];
    }
    else if (!gCollapsedHeaderViewMode && !gBuiltExpandedView)
    {
      if (headerName in gExpandedHeaderView)
        headerEntry = gExpandedHeaderView[headerName];

      if (!headerEntry && gViewAllHeaders)
      {
        // for view all headers, if we don't have a header field for this value....cheat and create one....then
        // fill in a headerEntry
        gExpandedHeaderView[headerName] = new createNewHeaderView(headerName);
        headerEntry = gExpandedHeaderView[headerName];
      }
    } // if we are in expanded view....

    if (headerEntry)
    {  
      headerEntry.outputFunction(headerEntry, headerField.headerValue);
      headerEntry.valid = true;    
    }
  }

  if (gCollapsedHeaderViewMode)
   gBuiltCollapsedView = true;
  else
   gBuiltExpandedView = true;

  // now update the view to make sure the right elements are visible
  updateHeaderViews();
  
  if ("FinishEmailProcessing" in this)
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
      node.collapsed = false;
  }
  else
  {
    node = document.getElementById("expandedHeaderView");
    if (node)
      node.collapsed = false;
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
    node.collapsed = true;

  node = document.getElementById("expandedHeaderView");
  if (node)
    node.collapsed = true;

  // we also have to disable the File/Attachments menuitem
  node = document.getElementById("fileAttachmentMenu");
  if (node)
    node.setAttribute("disabled", "true");
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
    var addresses = {};
    var fullNames = {};
    var names = {};
    var numAddresses =  0;

    numAddresses = msgHeaderParser.parseHeadersWithArray(emailAddresses, addresses, names, fullNames);
    var index = 0;
    while (index < numAddresses)
    {
      // if we want to include short/long toggle views and we have a long view, always add it.
      // if we aren't including a short/long view OR if we are and we haven't parsed enough
      // addresses to reach the cutoff valve yet then add it to the default (short) div.
      if (headerEntry.useToggle)
      {
        var address = {};
        address.emailAddress = addresses.value[index];
        address.fullAddress = fullNames.value[index];
        address.displayName = names.value[index];
        headerEntry.enclosingBox.addAddressView(address);
      }
      else
      {
        updateEmailAddressNode(headerEntry.enclosingBox.emailAddressNode, addresses.value[index], 
                               fullNames.value[index], names.value[index], headerEntry.useShortView);
      }

      if (headerEntry.enclosingBox.getAttribute("id") == "expandedfromBox") {
        setFromBuddyIcon(addresses.value[index]);
      }

      index++;
    }
    
    if (headerEntry.useToggle)
      headerEntry.enclosingBox.buildViews(gNumAddressesToShow);
  } // if msgheader parser
}

function setFromBuddyIcon(email)
{
   var fromBuddyIcon = document.getElementById("fromBuddyIcon");

   try {
     // better to cache this?
     var myScreenName = pref.getCharPref("aim.session.screenname");

     var card = abAddressCollector.getCardFromAttribute("PrimaryEmail", email);

     if (myScreenName && card && card.aimScreenName) {
       if (!gIOService) {
         // lazily create these globals
         gIOService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
         gFileHandler = gIOService.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
         
         var profile = Components.classes["@mozilla.org/profile/manager;1"].getService(Components.interfaces.nsIProfileInternal);
         gProfileDirURL = gIOService.newFileURI(profile.getProfileDir(profile.currentProfile));
       }

       // if we did have a buddy icon on disk for this screenname, this would be the file url spec for it
       var iconURLStr = gProfileDirURL.spec + "/NIM/" + myScreenName + "/picture/" + card.aimScreenName + ".gif";

       // check if the file exists
       // is this a perf hit?  (how expensive is stat()?)
       var iconFile = gFileHandler.getFileFromURLSpec(iconURLStr);
       if (iconFile.exists()) {
         fromBuddyIcon.setAttribute("src", iconURLStr);
         return;
       }
     }
   }
   catch (ex) {
     // can get here if no screenname
     //dump("ex = " + ex + "\n");
   }
   fromBuddyIcon.setAttribute("src", "");
}

function updateEmailAddressNode(emailAddressNode, emailAddress, fullAddress, displayName, useShortView)
{
  if (useShortView && displayName) {
    emailAddressNode.setAttribute("label", displayName);
    emailAddressNode.setAttribute("tooltiptext", emailAddress);
  } else {
    emailAddressNode.setAttribute("label", fullAddress);
    emailAddressNode.removeAttribute("tooltiptext");
  }
  emailAddressNode.setTextAttribute("emailAddress", emailAddress);
  emailAddressNode.setTextAttribute("fullAddress", fullAddress);  
  emailAddressNode.setTextAttribute("displayName", displayName);  
  
  if ("AddExtraAddressProcessing" in this)
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
                      "chrome,resizable=no,titlebar,modal,centerscreen", 
                      {primaryEmail:primaryEmail, displayName:displayName });
  }
}

// SendMailToNode takes the email address title button, extracts
// the email address we stored in there and opens a compose window
// with that address
function SendMailToNode(emailAddressNode)
{
  var fields = Components.classes["@mozilla.org/messengercompose/composefields;1"].createInstance(Components.interfaces.nsIMsgCompFields);
  var params = Components.classes["@mozilla.org/messengercompose/composeparams;1"].createInstance(Components.interfaces.nsIMsgComposeParams);
  if (emailAddressNode && fields && params)
  {
    fields.to = emailAddressNode.getAttribute("fullAddress");
    params.type = Components.interfaces.nsIMsgCompType.New;
    params.format = Components.interfaces.nsIMsgCompFormat.Default;
    params.identity = accountManager.getFirstIdentityForServer(GetLoadedMsgFolder().server);
    params.composeFields = fields;
    msgComposeService.OpenComposeWindowWithParams(null, params);
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

createNewAttachmentInfo.prototype.saveAttachment = function saveAttachment()
{
  messenger.saveAttachment(this.contentType, 
                           this.url, 
                           encodeURIComponent(this.displayName), 
                           this.uri);
}

createNewAttachmentInfo.prototype.openAttachment = function openAttachment()
{
  messenger.openAttachment(this.contentType, 
                           this.url, 
                           encodeURIComponent(this.displayName), 
                           this.uri);
}

createNewAttachmentInfo.prototype.printAttachment = function printAttachment()
{
  /* we haven't implemented the ability to print attachments yet...
  messenger.printAttachment(this.contentType, 
                            this.url, 
                            encodeURIComponent(this.displayName), 
                            this.uri);
  */
}

function onShowAttachmentContextMenu()
{
  // if no attachments are selected, disable the Open and Save...
  var attachmentList = document.getElementById('attachmentList');
  var selectedAttachments = attachmentList.selectedItems;
  var openMenu = document.getElementById('context-openAttachment');
  var saveMenu = document.getElementById('context-saveAttachment');
  if (selectedAttachments.length > 0)
  {
    openMenu.removeAttribute('disabled');
    saveMenu.removeAttribute('disabled');
  }
  else
  {
    openMenu.setAttribute('disabled', true);
    saveMenu.setAttribute('disabled', true);
  }
}

// this is our onclick handler for the attachment list. 
// A double click in a listitem simulates "opening" the attachment....
function attachmentListClick(event)
{ 
    // we only care about button 0 (left click) events
    if (event.button != 0) return;

    if (event.detail == 2) // double click
    {
      var target = event.target;
      if (target.localName == "listitem")
      {
	target.attachment.openAttachment();
      }
    }
}

// on command handlers for the attachment list context menu...
// commandPrefix matches one of our existing functions 
// (openAttachment, saveAttachment, etc.)
function handleAttachmentSelection(commandPrefix)
{
  var attachmentList = document.getElementById('attachmentList');
  var selectedAttachments = attachmentList.selectedItems;
  var listItem = selectedAttachments[0];

  listItem.attachment[commandPrefix]();
}

function displayAttachmentsForExpandedView()
{
  var numAttachments = currentAttachments.length;
  if (numAttachments > 0 && !gBuildAttachmentsForCurrentMsg)
  {
    var attachmentList = document.getElementById('attachmentList');

    for (index in currentAttachments)
    {
      var attachment = currentAttachments[index];

      // we need to create a listitem to insert the attachment
      // into the attachment list..
      var item = attachmentList.appendItem(attachment.displayName,"");
      item.setAttribute("class", "listitem-iconic"); 
      item.setAttribute("tooltip", "attachmentListTooltip");
      item.attachment = attachment;
      item.setAttribute("attachmentUrl", attachment.url);
      item.setAttribute("attachmentContentType", attachment.contentType);
      item.setAttribute("attachmentUri", attachment.uri);
      setApplicationIconForAttachment(attachment, item);
    } // for each attachment   

    gBuildAttachmentsForCurrentMsg = true;
  }
   
  var expandedAttachmentBox = document.getElementById('expandedAttachmentBox');
  expandedAttachmentBox.collapsed = numAttachments <= 0;
}

// attachment --> the attachment struct containing all the information on the attachment
// listitem --> the listitem currently showing the attachment.
function setApplicationIconForAttachment(attachment, listitem)
{
   // generate a moz-icon url for the attachment so we'll show a nice icon next to it.
   listitem.setAttribute('image', "moz-icon:" + "//" + attachment.displayName + "?size=16&contentType=" + attachment.contentType);
}

function displayAttachmentsForCollapsedView()
{
  var numAttachments = currentAttachments.length;
  var attachmentNode = document.getElementById('collapsedAttachmentBox');
  attachmentNode.collapsed = numAttachments <= 0; // make sure the attachment button is visible
}

// Public method called to generate a tooltip over an attachment
function FillInAttachmentTooltip(cellNode)
{
  var attachmentName = cellNode.getAttribute("label");
  var tooltipNode = document.getElementById("attachmentListTooltip");
  tooltipNode.setAttribute("label", attachmentName);
  return true;
}

// Public method called when we create the attachments file menu
function FillAttachmentListPopup(popup)
{
  // the FE sometimes call this routine TWICE...I haven't been able to figure out why yet...
  // protect against it...

  if (!gBuildAttachmentPopupForCurrentMsg) return; 

  var attachmentIndex = 0;

  // otherwise we need to build the attachment view...
  // First clear out the old view...
  ClearAttachmentMenu(popup);

  for (index in currentAttachments)
  {
    ++attachmentIndex;
    addAttachmentToPopup(popup, currentAttachments[index], attachmentIndex);
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
function addAttachmentToPopup(popup, attachment, attachmentIndex) 
{ 
  if (popup)
  { 
    var item = document.createElement('menu');
    if ( item ) 
    {     
      if (!gMessengerBundle)
        gMessengerBundle = document.getElementById("bundle_messenger");

      // insert the item just before the separator...the separator is the 2nd to last element in the popup.
      item.setAttribute('class', 'menu-iconic');
      setApplicationIconForAttachment(attachment,item);
      var numItemsInPopup = popup.childNodes.length;
      item = popup.insertBefore(item, popup.childNodes[numItemsInPopup-2]);

      var formattedDisplayNameString = gMessengerBundle.getFormattedString("attachmentDisplayNameFormat",
                                       [attachmentIndex, attachment.displayName]);

      item.setAttribute('label', formattedDisplayNameString); 
      item.setAttribute('accesskey', attachmentIndex); 

      var openpopup = document.createElement('menupopup');
      openpopup = item.appendChild(openpopup);

      var menuitementry = document.createElement('menuitem');     

      menuitementry.attachment = attachment;
      menuitementry.setAttribute('oncommand', 'this.attachment.openAttachment()'); 

      if (!gSaveLabel)
        gSaveLabel = gMessengerBundle.getString("saveLabel");
      if (!gSaveLabelAccesskey)
        gSaveLabelAccesskey = gMessengerBundle.getString("saveLabelAccesskey");
      if (!gOpenLabel)
        gOpenLabel = gMessengerBundle.getString("openLabel");
      if (!gOpenLabelAccesskey)
        gOpenLabelAccesskey = gMessengerBundle.getString("openLabelAccesskey");

      menuitementry.setAttribute('label', gOpenLabel); 
      menuitementry.setAttribute('accesskey', gOpenLabelAccesskey); 
      menuitementry = openpopup.appendChild(menuitementry);

      var menuseparator = document.createElement('menuseparator');
      openpopup.appendChild(menuseparator);
      
      menuitementry = document.createElement('menuitem');
      menuitementry.attachment = attachment;
      menuitementry.setAttribute('oncommand', 'this.attachment.saveAttachment()'); 
      menuitementry.setAttribute('label', gSaveLabel); 
      menuitementry.setAttribute('accesskey', gSaveLabelAccesskey); 
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
     attachmentDisplayNameArray[index] = encodeURI(attachment.displayName);
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

function ClearAttachmentList() 
{ 
  // we also have to disable the File/Attachments menuitem
  node = document.getElementById("fileAttachmentMenu");
  if (node)
    node.setAttribute("disabled", "true");

  // clear selection
  var list = document.getElementById('attachmentList');
  list.clearSelection();

  while (list.childNodes.length) 
    list.removeItemAt(list.childNodes.length - 1);
}

function ShowEditMessageButton() 
{
  var editBox = document.getElementById("editMessageBox");
  if (editBox)
    editBox.collapsed = false;
} 

function ClearEditMessageButton() 
{ 
  var editBox = document.getElementById("editMessageBox");
  if (editBox)
    editBox.collapsed = true;
}


var attachmentAreaDNDObserver = {
  onDragStart: function (aEvent, aAttachmentData, aDragAction)
  {
    var target = aEvent.target;
    if (target.localName == "listitem")
    {
      var attachmentUrl = target.getAttribute("attachmentUrl");
      var attachmentDisplayName = target.getAttribute("label");
      var attachmentContentType = target.getAttribute("attachmentContentType");
      var tmpurl = attachmentUrl;
      var tmpurlWithExtraInfo = tmpurl + "&type=" + attachmentContentType + "&filename=" + attachmentDisplayName;
      aAttachmentData.data = new TransferData();
      if (attachmentUrl && attachmentDisplayName)
      {
        aAttachmentData.data.addDataForFlavour("text/x-moz-url", tmpurlWithExtraInfo + "\n" + attachmentDisplayName);
        aAttachmentData.data.addDataForFlavour("text/x-moz-url-data", tmpurl);
        aAttachmentData.data.addDataForFlavour("text/x-moz-url-desc", attachmentDisplayName);
        
        aAttachmentData.data.addDataForFlavour("application/x-moz-file-promise-url", tmpurl);   
        aAttachmentData.data.addDataForFlavour("application/x-moz-file-promise", new nsFlavorDataProvider(), 0, Components.interfaces.nsISupports);     
      }
    }
  }
};

function nsFlavorDataProvider()
{
}

nsFlavorDataProvider.prototype =
{
  QueryInterface : function(iid)
  {
      if (iid.equals(Components.interfaces.nsIFlavorDataProvider) ||
          iid.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_NOINTERFACE;
  },
  
  getFlavorData : function(aTransferable, aFlavor, aData, aDataLen)
  {

    // get the url for the attachment
    if (aFlavor == "application/x-moz-file-promise")
    {
      var urlPrimitive = { };
      var dataSize = { };
      aTransferable.getTransferData("application/x-moz-file-promise-url", urlPrimitive, dataSize);

      var srcUrlPrimitive = urlPrimitive.value.QueryInterface(Components.interfaces.nsISupportsString);

      // now get the destination file location from kFilePromiseDirectoryMime
      var dirPrimitive = {};
      aTransferable.getTransferData("application/x-moz-file-promise-dir", dirPrimitive, dataSize);
      var destDirectory = dirPrimitive.value.QueryInterface(Components.interfaces.nsILocalFile);

      // now save the attachment to the specified location
      // XXX: we need more information than just the attachment url to save it, fortunately, we have an array
      // of all the current attachments so we can cheat and scan through them

      var attachment = null;
      for (index in currentAttachments)
      {
        attachment = currentAttachments[index];
        if (attachment.url == srcUrlPrimitive)
          break;
      }

      // call our code for saving attachments
      if (attachment)
      {
        var destFilePath = messenger.saveAttachmentToFolder(attachment.contentType, attachment.url, attachment.displayName, attachment.uri, destDirectory);
        aData.value = destFilePath.QueryInterface(Components.interfaces.nsISupports);
        aDataLen.value = 4;
      }
    }
  }

}



