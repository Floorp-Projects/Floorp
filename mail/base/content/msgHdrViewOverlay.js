# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****


/* This is where functions related to displaying the headers for a selected message in the
   message pane live. */

////////////////////////////////////////////////////////////////////////////////////
// Warning: if you go to modify any of these JS routines please get a code review from
// scott@scott-macgregor.org. It's critical that the code in here for displaying
// the message headers for a selected message remain as fast as possible. In particular, 
// right now, we only introduce one reflow per message. i.e. if you click on a message in the thread
// pane, we batch up all the changes for displaying the header pane (to, cc, attachements button, etc.) 
// and we make a single pass to display them. It's critical that we maintain this one reflow per message
// view in the message header pane. 
////////////////////////////////////////////////////////////////////////////////////

const msgHeaderParserContractID		   = "@mozilla.org/messenger/headerparser;1";
const abAddressCollectorContractID	 = "@mozilla.org/addressbook/services/addressCollecter;1";
const kPersonalAddressbookUri        = "moz-abmdbdirectory://abook.mab";
const kRDFServiceContractID          = "@mozilla.org/rdf/rdf-service;1";

const kLargeIcon = 32;
const kSmallIcon = 16;

var gViewAllHeaders = false;
var gShowOrganization = false;
var gShowLargeAttachmentView = false;
var gShowUserAgent = false;
var gExtraExpandedHeaders;
var gMinNumberOfHeaders = 0;
var gDummyHeaderIdIndex = 0;
var gCollapsedHeaderViewMode = false;
var gBuildAttachmentsForCurrentMsg = false;
var gBuildAttachmentPopupForCurrentMsg = true;
var gBuiltExpandedView = false;
var gBuiltCollapsedView = false;
var gOpenLabel;
var gOpenLabelAccesskey;
var gSaveLabel;
var gSaveLabelAccesskey;
var gDetachLabel;
var gDeleteLabel;
var gDetachLabelAccesskey;
var gDeleteLabelAccesskey;
var gMessengerBundle;
var gProfileDirURL;
var gIOService;
var gShowCondensedEmailAddresses = true; // show the friendly display names for people I know instead of the name + email address
var gPersonalAddressBookDirectory; // used for determining if we want to show just the display name in email address nodes

var msgHeaderParser = Components.classes[msgHeaderParserContractID].getService(Components.interfaces.nsIMsgHeaderParser);
var abAddressCollector = null;

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
                             {name:"from", useToggle:true, useShortView:true, outputFunction:OutputEmailAddresses},
                             {name:"date", outputFunction:updateHeaderValueInTextNode}];

// We also have an expanded header view. This shows many of your more common (and useful) headers.
var gExpandedHeaderList = [ {name:"subject"}, 
                            {name:"from", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"sender", outputFunction:OutputEmailAddresses},
                            {name:"reply-to", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"date"},
                            {name:"to", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"cc", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"bcc", useToggle:true, outputFunction:OutputEmailAddresses},
                            {name:"newsgroups", outputFunction:OutputNewsgroups},
                            {name:"followup-to", outputFunction:OutputNewsgroups},
                            {name:"content-base"},
                            {name:"tags"} ];

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
// isExternalAttachment --> boolean flag stating whether the attachment is an attachment which is a URL that refers to the attachment location
var currentAttachments = new Array();

// createHeaderEntry --> our constructor method which creates a header Entry 
// based on an entry in one of the header lists. A header entry is different from a header list.
// a header list just describes how you want a particular header to be presented. The header entry
// actually has knowledge about the DOM and the actual DOM elements associated with the header.
// prefix --> the name of the view (i.e. "collapsed", "expanded")
// headerListInfo --> entry from a header list.
function createHeaderEntry(prefix, headerListInfo)
{
  var useShortView = false;
  var partialIDName = prefix + headerListInfo.name;
  this.enclosingBox = document.getElementById(partialIDName + 'Box');
  this.textNode = document.getElementById(partialIDName + 'Value');
  this.isValid = false;

  if ("useShortView" in headerListInfo)
  {
    useShortView = headerListInfo.useShortView;
    if (useShortView)
      this.enclosingBox = this.textNode;
    else
      this.enclosingBox.emailAddressNode = this.textNode;
  }

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

  if (this.textNode)
    this.textNode.useShortView = useShortView;

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
    
    var extraHeaders = gExtraExpandedHeaders.split(' ');
    for (index = 0; index < extraHeaders.length; index++)
    {
      var extraHeader = extraHeaders[index];
      gExpandedHeaderView[extraHeader.toLowerCase()] = new createNewHeaderView(extraHeader, extraHeader + ':');
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
  gShowUserAgent = pref.getBoolPref("mailnews.headers.showUserAgent");
  gMinNumberOfHeaders = pref.getIntPref("mailnews.headers.minNumHeaders");
  gShowOrganization = pref.getBoolPref("mailnews.headers.showOrganization");
  gShowLargeAttachmentView = pref.getBoolPref("mailnews.attachments.display.largeView");
  gShowCondensedEmailAddresses = pref.getBoolPref("mail.showCondensedAddresses");
  gExtraExpandedHeaders = pref.getCharPref("mailnews.headers.extraExpandedHeaders");

  // listen to the 
  pref.addObserver("mail.showCondensedAddresses", MsgHdrViewObserver, false);

  initializeHeaderViewTables();

  var deckHeaderView = document.getElementById("msgHeaderViewDeck");
  gCollapsedHeaderViewMode = deckHeaderView.selectedIndex == 0;   

  // dispatch an event letting any listeners know that we have loaded the message pane
  var event = document.createEvent('Events');
  event.initEvent('messagepane-loaded', false, true);
  var headerViewElement = document.getElementById("msgHeaderView");
  headerViewElement.dispatchEvent(event);
}

function OnUnloadMsgHeaderPane()
{
  pref.removeObserver("mail.showCondensedAddresses", MsgHdrViewObserver);

  // dispatch an event letting any listeners know that we have unloaded the message pane
  var event = document.createEvent('Events');
  event.initEvent('messagepane-unloaded', false, true);
  var headerViewElement = document.getElementById("msgHeaderView");
  headerViewElement.dispatchEvent(event);
}

const MsgHdrViewObserver = 
{
  observe: function(subject, topic, prefName)
  {
    // verify that we're changing the mail pane config pref
    if (topic == "nsPref:changed")
    {
      if (prefName == "mail.showCondensedAddresses")
      {
        gShowCondensedEmailAddresses = pref.getBoolPref("mail.showCondensedAddresses");
        MsgReload();
      }
    }
  }
};

// The messageHeaderSink is the class that gets notified of a message's headers as we display the message
// through our mime converter. 

var messageHeaderSink = {
    onStartHeaders: function()
    {
      this.mSaveHdr = null;
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
      gMessageNotificationBar.clearMsgNotifications();

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
      ShowEditMessageButton();
      
      for (index in gMessageListeners)
        gMessageListeners[index].onEndHeaders();
    },
    
    processHeaders: function(headerNameEnumerator, headerValueEnumerator, dontCollectAddress)
    {
      this.onStartHeaders(); 

      const kMailboxSeparator = ", ";
      var index = 0;
      while (headerNameEnumerator.hasMore()) 
      {
        var header = new Object;        
        header.headerValue = headerValueEnumerator.getNext();
        header.headerName = headerNameEnumerator.getNext();

        // for consistancy sake, let's force all header names to be lower case so
        // we don't have to worry about looking for: Cc and CC, etc.
        var lowerCaseHeaderName = header.headerName.toLowerCase();

        // if we have an x-mailer or x-mimeole string, put it in the user-agent slot which we know how to handle
        // already. 
        if (lowerCaseHeaderName == "x-mailer" || lowerCaseHeaderName == "x-mimeole")
          lowerCaseHeaderName = "user-agent";   

        if (this.mDummyMsgHeader)
        {
          if (lowerCaseHeaderName == "from")
            this.mDummyMsgHeader.author = header.headerValue;
          else if (lowerCaseHeaderName == "to")
            this.mDummyMsgHeader.recipients = header.headerValue;
          else if (lowerCaseHeaderName == "cc")
            this.mDummyMsgHeader.ccList = header.headerValue;
          else if (lowerCaseHeaderName == "subject")
            this.mDummyMsgHeader.subject = header.headerValue;
          else if (lowerCaseHeaderName == "reply-to")
            this.mDummyMsgHeader.replyTo = header.headerValue;
          else if (lowerCaseHeaderName == "message-id")
            this.mDummyMsgHeader.messageId = header.headerValue;

        }
        // according to RFC 2822, certain headers
        // can occur "unlimited" times
        if (lowerCaseHeaderName in currentHeaderData)
        {
          // sometimes, you can have multiple To or Cc lines....
          // in this case, we want to append these headers into one.
          if (lowerCaseHeaderName == 'to' || lowerCaseHeaderName == 'cc')
            currentHeaderData[lowerCaseHeaderName].headerValue = currentHeaderData[lowerCaseHeaderName].headerValue + ',' + header.headerValue;
          else {  
            // use the index to create a unique header name like:
            // received5, received6, etc
            currentHeaderData[lowerCaseHeaderName + index++] = header;
          }
        }
        else
         currentHeaderData[lowerCaseHeaderName] = header;
      } // while we have more headers to parse
      
      // process message tags as if they were headers in the message
      SetTagHeader();      

      if (("from" in currentHeaderData) && ("sender" in currentHeaderData) && msgHeaderParser)
      {
        var senderMailbox = kMailboxSeparator + msgHeaderParser.extractHeaderAddressMailboxes(null,
                            currentHeaderData.sender.headerValue) + kMailboxSeparator;
        var fromMailboxes = kMailboxSeparator + msgHeaderParser.extractHeaderAddressMailboxes(null,
                            currentHeaderData.from.headerValue) + kMailboxSeparator;
        if (fromMailboxes.indexOf(senderMailbox) >= 0)
          delete currentHeaderData.sender;
      }

      this.onEndHeaders();
    },
    
    handleAttachment: function(contentType, url, displayName, uri, isExternalAttachment) 
    {
      // presentation level change....don't show vcards as external attachments in the UI.
      // libmime already renders them inline.

      try
      {
        if (!this.mSaveHdr)
          this.mSaveHdr = messenger.messageServiceFromURI(uri).messageURIToMsgHdr(uri);
      }
      catch (ex) {}
      if (contentType == "text/x-vcard")
      {
        var inlineAttachments = pref.getBoolPref("mail.inline_attachments");
        var displayHtmlAs = pref.getIntPref("mailnews.display.html_as");
        if (inlineAttachments && !displayHtmlAs)
        {
          return;
        }
      }

      // display name optimization. Eliminate any large quantities of white space from the display name.
      // such that Hello       World.txt becomes Hello World.txt.
      displayName = displayName.replace(/ +/g, " ");
          
      currentAttachments.push (new createNewAttachmentInfo(contentType, url, displayName, uri, isExternalAttachment));
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
          this.mSaveHdr.markHasAttachments(true);
        }
        catch (ex) {
          dump("ex = " + ex + "\n");
        }
      }
    },
    
    onEndAllAttachments: function()
    {
      displayAttachmentsForExpandedView();
    },

    onEndMsgDownload: function(url)
    {
      // if we don't have any attachments, turn off the attachments flag
      if (!this.mSaveHdr)
      {
        var messageUrl = url.QueryInterface(Components.interfaces.nsIMsgMessageUrl);
        try
        {
          this.mSaveHdr = messenger.msgHdrFromURI(messageUrl.uri);
        }
        catch (ex) {}

      }
      if (!currentAttachments.length && this.mSaveHdr)
        this.mSaveHdr.markHasAttachments(false);
      OnMsgParsed(url);
    },

    onEndMsgHeaders: function(url)
    { 
      OnMsgLoaded(url);
    },

    onMsgHasRemoteContent: function(aMsgHdr)
    {
      gMessageNotificationBar.setRemoteContentMsg(aMsgHdr);
    },

    mSecurityInfo  : null,
    mSaveHdr: null,
    get securityInfo()
    {
      return this.mSecurityInfo;
    },
    set securityInfo(aSecurityInfo)
    {
      this.mSecurityInfo = aSecurityInfo;
    },

    mDummyMsgHeader: null,

    get dummyMsgHeader()
    {
      if (!this.mDummyMsgHeader)
        this.mDummyMsgHeader = new nsDummyMsgHeader();
      return this.mDummyMsgHeader;
    },
    mProperties: null,
    get properties()
    {
      if (!this.mProperties)
        this.mProperties = Components.classes["@mozilla.org/hash-property-bag;1"].
          createInstance(Components.interfaces.nsIWritablePropertyBag2);
      return this.mProperties;
    }
};

function SetTagHeader()
{
  // it would be nice if we passed in the msgHdr from the back end
  var msgHdr;
  try
  {
    msgHdr = gDBView.hdrForFirstSelectedMessage;
  }
  catch (ex)
  {
    return; // no msgHdr to add our tags to
  }

  // get the list of known tags
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                   .getService(Components.interfaces.nsIMsgTagService);
  var tagArray = tagService.getAllTags({});
  var tagKeys = {};
  for each (var tagInfo in tagArray)
    if (tagInfo.tag)
      tagKeys[tagInfo.key] = true;

  // extract the tag keys from the msgHdr
  var msgKeyArray = msgHdr.getStringProperty("keywords").split(" ");

  // attach legacy label to the front if not already there
  var label = msgHdr.label;
  if (label)
  {
    var labelKey = "$label" + label;
    if (msgKeyArray.indexOf(labelKey) < 0)
      msgKeyArray.unshift(labelKey);
  }

 // Rebuild the keywords string with just the keys that are actual tags or
  // legacy labels and not other keywords like Junk and NonJunk.
  // Retain their order, though, with the label as oldest element.
  for (var i = msgKeyArray.length - 1; i >= 0; --i)
    if (!(msgKeyArray[i] in tagKeys))
      msgKeyArray.splice(i, 1); // remove non-tag key
  var msgKeys = msgKeyArray.join(" ");

  if (msgKeys)
    currentHeaderData.tags = {headerName: "tags", headerValue: msgKeys};
  else // no more tags, so clear out the header field
    delete currentHeaderData.tags;
}

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

function OnTagsChange()
{
  // rebuild the tag headers
  SetTagHeader();

  // now update the expanded header view to rebuild the tags,
  // and then show or hide the tag header box.  
  if (gBuiltExpandedView)
  {
    var headerEntry = gExpandedHeaderView.tags;
    if (headerEntry)
    {
      headerEntry.valid = ("tags" in currentHeaderData);
      if (headerEntry.valid)
        headerEntry.outputFunction(headerEntry, currentHeaderData.tags.headerValue);
      
      // if we are showing the expanded header view then we may need to collapse or
      // show the tag header box...
      if (!gCollapsedHeaderViewMode)
        headerEntry.enclosingBox.collapsed = !headerEntry.valid;
    }
  }
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

// enumerate through the list of headers and find the number that are visible
// add empty entries if we don't have the minimum number of rows
function EnsureMinimumNumberOfHeaders (headerTable)
{ 
  if (!gMinNumberOfHeaders) // 0 means we don't have a minimum..do nothing special
    return;

  var numVisibleHeaders = 0;
  for (index in headerTable)
  {
    if (headerTable[index].valid)
      numVisibleHeaders ++;
  } 

  if (numVisibleHeaders < gMinNumberOfHeaders)
  { 
    // how many empty headers do we need to add?
    var numEmptyHeaders = gMinNumberOfHeaders - numVisibleHeaders;

    // we may have already dynamically created our empty rows and we just need to make them visible
    for (index in headerTable)
    { 
      if (index.indexOf("Dummy-Header") == 0 && numEmptyHeaders)
      {
        headerTable[index].valid = true;
        numEmptyHeaders--;
      }
    }

    // ok, now if we have any extra dummy headers we need to add, create a new header widget for them
    while (numEmptyHeaders)
    {
      var dummyHeaderId = "Dummy-Header" + gDummyHeaderIdIndex;
      gExpandedHeaderView[dummyHeaderId] = new createNewHeaderView(dummyHeaderId, "");
      gExpandedHeaderView[dummyHeaderId].valid = true;

      gDummyHeaderIdIndex++;
      numEmptyHeaders--;
    }

  }
}

// make sure the appropriate fields within the currently displayed view header mode
// are collapsed or visible...
function updateHeaderViews()
{
  if (gCollapsedHeaderViewMode)
    showHeaderView(gCollapsedHeaderView);
  else
  {
    if (gMinNumberOfHeaders)
      EnsureMinimumNumberOfHeaders(gExpandedHeaderView);
    showHeaderView(gExpandedHeaderView);
  }

  displayAttachmentsForExpandedView();
}

function ToggleHeaderView ()
{
  gCollapsedHeaderViewMode = !gCollapsedHeaderViewMode;
  // Work around a xul deck bug where the height of the deck is determined by the tallest panel in the deck
  // even if that panel is not selected...
  document.getElementById('msgHeaderViewDeck').selectedPanel.collapsed = true;
  
  UpdateMessageHeaders(); 
  
  // select the new panel. 
  document.getElementById('msgHeaderViewDeck').selectedIndex = gCollapsedHeaderViewMode ? 0 : 1;

  // Work around a xul deck bug where the height of the deck is determined by the tallest panel in the deck
  // even if that panel is not selected...  
  document.getElementById('msgHeaderViewDeck').selectedPanel.collapsed = false;
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

function createNewHeaderView(headerName, label)
{
  var idName = 'expanded' + headerName + 'Box';
  var newHeader = document.createElement("mail-headerfield");
  
  newHeader.setAttribute('id', idName);
  newHeader.setAttribute('label', label);
  newHeader.collapsed = true;

  // this new element needs to be inserted into the view...
  var topViewNode = document.getElementById('expandedHeaders');

  topViewNode.appendChild(newHeader);
  
  this.enclosingBox = newHeader;
  this.isValid = false;
  this.useToggle = false;
  this.outputFunction = updateHeaderValue;
}

// UpdateMessageHeaders: Iterate through all the current header data we received from mime for this message
// for each header entry table, see if we have a corresponding entry for that header. i.e. does the particular
// view care about this header value. if it does then call updateHeaderEntry
function UpdateMessageHeaders()
{
  // iterate over each header we received and see if we have a matching entry in each
  // header view table...

  var headerName;

  // Remove the height attr so that it redraws correctly. Works around a problem that
  // attachment-splitter causes if it's moved high enough to affect the header box:
  document.getElementById('msgHeaderView').removeAttribute('height');

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
        gExpandedHeaderView[headerName] = new createNewHeaderView(headerName, currentHeaderData[headerName].headerName + ':');
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
  document.getElementById('msgHeaderView').collapsed = false;

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
  document.getElementById('msgHeaderView').collapsed = true;

  // disable the File/Attachments menuitem
  document.getElementById("fileAttachmentMenu").setAttribute("disabled", "true");
  // disable the attachment box
  document.getElementById("attachmentView").collapsed = true;
  document.getElementById("attachment-splitter").collapsed = true;
}

function OutputNewsgroups(headerEntry, headerValue)
{ 
  headerValue = headerValue.replace(/,/g,", ");
  updateHeaderValue(headerEntry, headerValue);
}

// OutputEmailAddresses --> knows how to take a comma separated list of email addresses,
// extracts them one by one, linkifying each email address into a mailto url. 
// Then we add the link-ified email address to the parentDiv passed in.
// 
// emailAddresses --> comma separated list of the addresses for this header field

function OutputEmailAddresses(headerEntry, emailAddresses)
{
  if (!emailAddresses)
    return;

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
      var address = {};
      address.emailAddress = addresses.value[index];
      address.fullAddress = fullNames.value[index];
      address.displayName = names.value[index];
      if (headerEntry.useToggle)
        headerEntry.enclosingBox.addAddressView(address);
      else
        updateEmailAddressNode(headerEntry.enclosingBox.emailAddressNode, address);
      index++;
    }
    
    if (headerEntry.useToggle)
      headerEntry.enclosingBox.buildViews();
  } // if msgheader parser
}

function updateEmailAddressNode(emailAddressNode, address)
{
  emailAddressNode.setAttribute("label", address.fullAddress || address.displayName);
  emailAddressNode.removeAttribute("tooltiptext");
  emailAddressNode.setAttribute("emailAddress", address.emailAddress);
  emailAddressNode.setAttribute("fullAddress", address.fullAddress);
  emailAddressNode.setAttribute("displayName", address.displayName);

  AddExtraAddressProcessing(address.emailAddress, emailAddressNode);
}

// thunderbird has smart logic for determining if we should show just the display name.
// mozilla already has a generic method that gets called on each email address node called
// AddExtraAddressProcessing which is undefined in seamonkey. Let's hijack this convient method to do what
// we want.

function AddExtraAddressProcessing(emailAddress, addressNode)
{
  var displayName = addressNode.getAttribute("displayName");  
  var mailAddress = addressNode.getAttribute("emailAddress");

  // always show the address for the from and reply-to fields
  var parentElementId = addressNode.parentNode.id;
  var condenseName = true;
  if (parentElementId == "expandedfromBox" || parentElementId == "expandedreply-toBox")
    condenseName = false;

  if (condenseName && gShowCondensedEmailAddresses && displayName)
  {
    if (useDisplayNameForAddress(emailAddress))
      addressNode.setAttribute("label", displayName);
    addressNode.setAttribute("tooltiptext", mailAddress);
    addressNode.setAttribute("tooltip", "emailAddressTooltip");
  }
  else
    addressNode.removeAttribute("tooltiptext");
} 

function fillEmailAddressPopup(emailAddressNode)
{
  var emailAddressPlaceHolder = document.getElementById('emailAddressPlaceHolder');
  var emailAddress = emailAddressNode.getAttribute('emailAddress');

  emailAddressPlaceHolder.setAttribute('label', emailAddress);
}


// Public method called to generate a tooltip over a condensed display name
function fillInEmailAddressTooltip(addressNode)
{
  var emailAddress = addressNode.getAttribute('emailAddress');
  var tooltipNode = document.getElementById("attachmentListTooltip");
  tooltipNode.setAttribute("label", attachmentName);
  return true;
}

// returns true if we should use the display name for this address
// otherwise returns false
function useDisplayNameForAddress(emailAddress)
{
  // For now, if the email address is in the personal address book, then consider this user a 'known' user
  // and use the display name. I could eventually see our rules enlarged to include other local ABs, replicated
  // LDAP directories, and maybe even domain matches (i.e. any email from someone in my company
  // should use the friendly display name) 

  if (!gPersonalAddressBookDirectory)
  {
    var RDFService = Components.classes[kRDFServiceContractID].getService(Components.interfaces.nsIRDFService); 
    gPersonalAddressBookDirectory = RDFService.GetResource(kPersonalAddressbookUri).QueryInterface(Components.interfaces.nsIAbMDBDirectory);
      
    if (!gPersonalAddressBookDirectory)
      return false;
  }

  // look up the email address in the database
  return gPersonalAddressBookDirectory.hasCardForEmailAddress(emailAddress);
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
         top.MsgFilters(emailAddress, GetFirstSelectedMsgFolder());
     }
  }
}

// createnewAttachmentInfo --> constructor method for creating new attachment object which goes into the
// data attachment array.
function createNewAttachmentInfo(contentType, url, displayName, uri, isExternalAttachment)
{
  this.contentType = contentType;
  this.url = url;
  this.displayName = displayName;
  this.uri = uri;
  this.isExternalAttachment = isExternalAttachment;
}

function dofunc(aFunctionName, aFunctionArg)
{
  if (aFunctionName == "saveAttachment") 
    saveAttachment(aFunctionArg); 
  else if (aFunctionName == "openAttachment") 
    openAttachment(aFunctionArg); 
  else if (aFunctionName == "printAttachment") 
    printAttachment(aFunctionArg);
  else if (aFunctionName == "deleteAttachment")
    detachAttachment(aFunctionArg, false);
  else if (aFunctionName == "detachAttachment")
    detachAttachment(aFunctionArg, true);
}

function saveAttachment(aAttachment)
{
  messenger.saveAttachment(aAttachment.contentType, 
                           aAttachment.url, 
                           encodeURIComponent(aAttachment.displayName), 
                           aAttachment.messageUri, aAttachment.isExternalAttachment);
}

function openAttachment(aAttachment)
{
  messenger.openAttachment(aAttachment.contentType, 
                           aAttachment.url, 
                           encodeURIComponent(aAttachment.displayName), 
                           aAttachment.messageUri, aAttachment.isExternalAttachment);
}

function printAttachment(aAttachment)
{
  /* we haven't implemented the ability to print attachments yet...
  messenger.printAttachment(aAttachment.contentType, 
                            aAttachment.url, 
                            encodeURIComponent(aAttachment.displayName), 
                            aAttachment.messageUri);
  */
}

function detachAttachment(aAttachment, aSaveFirst)
{
  messenger.detachAttachment(aAttachment.contentType, 
                           aAttachment.url, 
                           encodeURIComponent(aAttachment.displayName), 
                           aAttachment.messageUri, aSaveFirst);
}

function CanDetachAttachments()
{
  var uri = GetLoadedMessage();
  var canDetach = !IsNewsMessage(uri) && (!IsImapMessage(uri) || MailOfflineMgr.isOnline());
  if (canDetach && ("content-type" in currentHeaderData))
  {
    var contentType = currentHeaderData["content-type"].headerValue;
    canDetach = contentType.indexOf("application/x-pkcs7-mime") < 0 && 
        contentType.indexOf("application/x-pkcs7-signature") < 0;
  }
  return canDetach;
}

function onShowAttachmentContextMenu()
{
  // if no attachments are selected, disable the Open and Save...
  var attachmentList = document.getElementById('attachmentList');
  var selectedAttachments = attachmentList.selectedItems;
  var openMenu = document.getElementById('context-openAttachment');
  var saveMenu = document.getElementById('context-saveAttachment');
  var detachMenu = document.getElementById('context-detachAttachment');
  var deleteMenu = document.getElementById('context-deleteAttachment');
  var detachAllMenu = document.getElementById('context-detachAllAttachments');
  var deleteAllMenu = document.getElementById('context-deleteAllAttachments');
  var canDetach = CanDetachAttachments();
  var canOpen = false;
  for (var i = 0; i < selectedAttachments.length && !canOpen; i++)
    canOpen = selectedAttachments[i].attachment.contentType != 'text/x-moz-deleted';
  if (canOpen && selectedAttachments.length == 1)
  {
    openMenu.removeAttribute('disabled');
  }
  else
  {
    openMenu.setAttribute('disabled', true);
  }
  if (canOpen)
  {
    saveMenu.removeAttribute('disabled');
  }
  else
  {
    saveMenu.setAttribute('disabled', true);
  }
  if (canDetach && canOpen)
  {
    detachMenu.removeAttribute('disabled');
    deleteMenu.removeAttribute('disabled');
  }
  else
  {
    detachMenu.setAttribute('disabled', 'true');
    deleteMenu.setAttribute('disabled', 'true');
  }
  if (canDetach)
  {
    detachAllMenu.removeAttribute('disabled');
    deleteAllMenu.removeAttribute('disabled');
  }
  else
  {
    detachAllMenu.setAttribute('disabled', 'true');
    deleteAllMenu.setAttribute('disabled', 'true');
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
    if (target.localName == "descriptionitem")
    {
      dofunc("openAttachment", target.attachment);
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

  // loop over all of the selected attachments...

  // XXX hack alert. If we sit in tight loop and call doFunc for multiple attachments, 
  // we get chrome errors in layout as we start loading the first helper app dialog
  // then before it loads, we kick off the next one and the next one. Subsequent helper app dialogs
  // were failing because we were still loading the chrome files for the first attempt (error about the xul cache
  // being empty). For now, work around this by doing the first helper app dialog right away, then waiting a bit
  // before we launch the rest.
  for (var i = 0; i < selectedAttachments.length; i++)
    if (!i)
      dofunc(commandPrefix, selectedAttachments[i].attachment);
    else
      setTimeout(dofunc, 100, commandPrefix, selectedAttachments[i].attachment);
}

function cloneAttachment(aAttachment)
{
  var obj = new Object();
  obj.contentType = aAttachment.contentType;
  obj.url = aAttachment.url;
  obj.displayName = aAttachment.displayName;
  obj.messageUri = aAttachment.uri;
  obj.isExternalAttachment = aAttachment.isExternalAttachment;
  return obj;
}

function displayAttachmentsForExpandedView()
{
  var numAttachments = currentAttachments.length; 
  var expandedAttachmentBox = document.getElementById('attachmentView');
  var attachmentSplitter = document.getElementById('attachment-splitter');

  if (numAttachments <= 0)
  {
    expandedAttachmentBox.collapsed = true;
    attachmentSplitter.collapsed = true;
  }
  else if (!gBuildAttachmentsForCurrentMsg)
  {
    // IMPORTANT: make sure we uncollapse the attachment box BEFORE we start adding
    // our attachments to the view. Otherwise, layout doesn't calculate the correct height for
    // the attachment view and we end up with a box that is too tall.
    expandedAttachmentBox.collapsed = false;
    attachmentSplitter.collapsed = false;
    
    if (gShowLargeAttachmentView)
      expandedAttachmentBox.setAttribute("largeView", "true");

    // Remove height attribute, or the attachments box could be drawn badly:
    expandedAttachmentBox.removeAttribute("height");

    var attachmentList = document.getElementById('attachmentList');
    for (index in currentAttachments)
    {
      var attachment = currentAttachments[index];

      // create a new attachment widget. set the label, set the 
      // moz-icon img src
      var attachmentView = attachmentList.appendItem(attachment.displayName);
      attachmentView.setAttribute("class", "descriptionitem-iconic"); 

      if (gShowLargeAttachmentView)
        attachmentView.setAttribute("largeView", "true");

      setApplicationIconForAttachment(attachment, attachmentView, gShowLargeAttachmentView);
      attachmentView.setAttribute("tooltip", "attachmentListTooltip");
      attachmentView.setAttribute("context", "attachmentListContext");      

      attachmentView.attachment = cloneAttachment(attachment);
      attachmentView.setAttribute("attachmentUrl", attachment.url);
      attachmentView.setAttribute("attachmentContentType", attachment.contentType);
      attachmentView.setAttribute("attachmentUri", attachment.uri);

      var item = attachmentList.appendChild(attachmentView);
    } // for each attachment
    gBuildAttachmentsForCurrentMsg = true;

    // Switch overflow off (via css attribute selector) temporarily to get the preferred window height:
    var attachmentContainer = document.getElementById('attachmentView');
    attachmentContainer.setAttribute("attachmentOverflow", "false");
    var attachmentHeight = expandedAttachmentBox.boxObject.height;
    attachmentContainer.setAttribute("attachmentOverflow", "true");

    // If the attachments box takes up too much of the message pane, downsize:
    var maxAttachmentHeight = document.getElementById('messagepanebox').boxObject.height / 4;
    if (attachmentHeight > maxAttachmentHeight)
      attachmentHeight = maxAttachmentHeight;
    expandedAttachmentBox.setAttribute("height", attachmentHeight);
 }
}

// attachment --> the attachment struct containing all the information on the attachment
// listitem --> the listitem currently showing the attachment.
function setApplicationIconForAttachment(attachment, listitem, largeView)
{
  var iconSize = largeView ? kLargeIcon : kSmallIcon;
  // generate a moz-icon url for the attachment so we'll show a nice icon next to it.
  if (attachment.contentType == 'text/x-moz-deleted')
    listitem.setAttribute('image', 'chrome://messenger/skin/icons/message-mail-attach-del.png');
  else
    listitem.setAttribute('image', "moz-icon:" + "//" + attachment.displayName + "?size=" + iconSize + "&contentType=" + attachment.contentType);
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
  var detachAllMenu = document.getElementById('file-detachAllAttachments');
  var deleteAllMenu = document.getElementById('file-deleteAllAttachments');
  if (CanDetachAttachments())
  {
    detachAllMenu.removeAttribute('disabled');
    deleteAllMenu.removeAttribute('disabled');
  }
  else
  {
    detachAllMenu.setAttribute('disabled', 'true');
    deleteAllMenu.setAttribute('disabled', 'true');
  }

}

// Public method used to clear the file attachment menu
function ClearAttachmentMenu(popup) 
{ 
  if ( popup ) 
  { 
     while ( popup.childNodes[0].localName == 'menu' )
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
      setApplicationIconForAttachment(attachment,item, false);
      
      var numItemsInPopup = popup.childNodes.length;
      // find the separator
      var indexOfSeparator = 0;
      while (popup.childNodes[indexOfSeparator].localName != 'menuseparator')
        indexOfSeparator++;

      var formattedDisplayNameString = gMessengerBundle.getFormattedString("attachmentDisplayNameFormat",
                                       [attachmentIndex, attachment.displayName]);

      item.setAttribute('label', formattedDisplayNameString); 
      item.setAttribute('accesskey', attachmentIndex); 

      // each attachment in the list gets its own menupopup with options for saving, deleting, detaching, etc. 
      var openpopup = document.createElement('menupopup');
      openpopup = item.appendChild(openpopup);

      // Due to Bug #314228, we must append our menupopup to the new attachment menu item
      // before we inserting the attachment menu into the popup. If we don't, our attachment
      // menu items will not show up.
      item = popup.insertBefore(item, popup.childNodes[indexOfSeparator]);

      var menuitementry = document.createElement('menuitem');
      menuitementry.attachment = cloneAttachment(attachment);
      menuitementry.setAttribute('oncommand', 'openAttachment(this.attachment)'); 

      if (!gSaveLabel)
        gSaveLabel = gMessengerBundle.getString("saveLabel");
      if (!gSaveLabelAccesskey)
        gSaveLabelAccesskey = gMessengerBundle.getString("saveLabelAccesskey");
      if (!gOpenLabel)
        gOpenLabel = gMessengerBundle.getString("openLabel");
      if (!gOpenLabelAccesskey)
        gOpenLabelAccesskey = gMessengerBundle.getString("openLabelAccesskey");
      if (!gDetachLabel)
        gDetachLabel = gMessengerBundle.getString("detachLabel");
      if (!gDetachLabelAccesskey)
        gDetachLabelAccesskey = gMessengerBundle.getString("detachLabelAccesskey");
      if (!gDeleteLabel)
        gDeleteLabel = gMessengerBundle.getString("deleteLabel");
      if (!gDeleteLabelAccesskey)
        gDeleteLabelAccesskey = gMessengerBundle.getString("deleteLabelAccesskey");

      // we should also check if an attachment has been detached...
      // but that uses X-Mozilla-External-Attachment-URL, which
      // we'd need to check for somehow.

      var signedOrEncrypted = false;
      if ("content-type" in currentHeaderData)
      {
        var contentType = currentHeaderData["content-type"].headerValue;
        signedOrEncrypted = contentType.indexOf("application/x-pkcs7-mime") >= 0 || 
            contentType.indexOf("application/x-pkcs7-signature") >= 0;
      }
      var canDetach = !(/news-message:/.test(attachment.uri)) && 
          !signedOrEncrypted &&
          (!(/imap-message/.test(attachment.uri)) || MailOfflineMgr.isOnline());
      menuitementry.setAttribute('label', gOpenLabel); 
      menuitementry.setAttribute('accesskey', gOpenLabelAccesskey); 
      menuitementry = openpopup.appendChild(menuitementry);
      if (attachment.contentType == 'text/x-moz-deleted')
        menuitementry.setAttribute('disabled', true); 

      var menuseparator = document.createElement('menuseparator');
      openpopup.appendChild(menuseparator);
      
      menuitementry = document.createElement('menuitem');
      menuitementry.attachment = cloneAttachment(attachment);
      menuitementry.setAttribute('oncommand', 'saveAttachment(this.attachment)'); 
      menuitementry.setAttribute('label', gSaveLabel); 
      menuitementry.setAttribute('accesskey', gSaveLabelAccesskey); 
      if (attachment.contentType == 'text/x-moz-deleted')
        menuitementry.setAttribute('disabled', true); 
      menuitementry = openpopup.appendChild(menuitementry);

      var menuseparator = document.createElement('menuseparator');
      openpopup.appendChild(menuseparator);

      menuitementry = document.createElement('menuitem');
      menuitementry.attachment = cloneAttachment(attachment);
      menuitementry.setAttribute('oncommand', 'detachAttachment(this.attachment, true)'); 
      menuitementry.setAttribute('label', gDetachLabel); 
      menuitementry.setAttribute('accesskey', gDetachLabelAccesskey); 
      if (attachment.contentType == 'text/x-moz-deleted' || !canDetach)
        menuitementry.setAttribute('disabled', true); 
      menuitementry = openpopup.appendChild(menuitementry);

      menuitementry = document.createElement('menuitem');
      menuitementry.attachment = cloneAttachment(attachment);
      menuitementry.setAttribute('oncommand', 'detachAttachment(this.attachment, false)'); 
      menuitementry.setAttribute('label', gDeleteLabel); 
      menuitementry.setAttribute('accesskey', gDeleteLabelAccesskey); 
      if (attachment.contentType == 'text/x-moz-deleted' || !canDetach)
        menuitementry.setAttribute('disabled', true); 
      menuitementry = openpopup.appendChild(menuitementry);
    }  // if we created a menu item for this attachment...
  } // if we have a popup
} 

function HandleAllAttachments(action)
{
 try 
 {
   // convert our attachment data into some c++ friendly structs
   var attachmentContentTypeArray = new Array();
   var attachmentUrlArray = new Array();
   var attachmentDisplayNameArray = new Array();
   var attachmentMessageUriArray = new Array();

   // populate these arrays..
   var actionIndex = 0;
   for (index in currentAttachments)
   {
     // exclude all attachments already deleted
     var attachment = currentAttachments[index];
     if ( attachment.contentType != 'text/x-moz-deleted' )
     {
       attachmentContentTypeArray[actionIndex] = attachment.contentType;
       attachmentUrlArray[actionIndex] = attachment.url;
       attachmentDisplayNameArray[actionIndex] = encodeURI(attachment.displayName);
       attachmentMessageUriArray[actionIndex] = attachment.uri;
       ++actionIndex;
     }
   }

   // okay the list has been built... now call our action code...
   if ( action == 'save' )
     messenger.saveAllAttachments(attachmentContentTypeArray.length,
                                  attachmentContentTypeArray, attachmentUrlArray,
                                  attachmentDisplayNameArray, attachmentMessageUriArray);
   else if ( action == 'detach' )
     messenger.detachAllAttachments(attachmentContentTypeArray.length,
                                    attachmentContentTypeArray, attachmentUrlArray,
                                    attachmentDisplayNameArray, attachmentMessageUriArray,
                                    true); // save
   else if ( action == 'delete' )
     messenger.detachAllAttachments(attachmentContentTypeArray.length,
                                    attachmentContentTypeArray, attachmentUrlArray,
                                    attachmentDisplayNameArray, attachmentMessageUriArray,
                                    false); // don't save
   else
     dump ("** unknown HandleAllAttachments action: " + action + "**\n");
   }
   catch (ex)
   {
   dump ("** failed to handle all attachments **\n");
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

  while (list.hasChildNodes()) 
    list.removeChild(list.lastChild);
}

function ShowEditMessageButton() 
{
  // it would be nice if we passed in the msgHdr from the back end
  var msgHdr;
  try
  {
    msgHdr = gDBView.hdrForFirstSelectedMessage;
  }
  catch (ex)
  {
    return;
  }

  if (IsSpecialFolder(msgHdr.folder, MSG_FOLDER_FLAG_DRAFTS, true))
    document.getElementById("editMessageBox").collapsed = false;
} 

function ClearEditMessageButton() 
{ 
  var editBox = document.getElementById("editMessageBox");
  if (editBox)
    editBox.collapsed = true;
}

// CopyWebsiteAddress takes the website address title button, extracts
// the website address we stored in there and copies it to the clipboard
function CopyWebsiteAddress(websiteAddressNode)
{
  if (websiteAddressNode)
  {
    var websiteAddress = websiteAddressNode.getAttribute("value");

    var contractid = "@mozilla.org/widget/clipboardhelper;1";
    var iid = Components.interfaces.nsIClipboardHelper;
    var clipboard = Components.classes[contractid].getService(iid);
    clipboard.copyString(websiteAddress);
  }
}

var attachmentAreaDNDObserver = {
  onDragStart: function (aEvent, aAttachmentData, aDragAction)
  {
    var target = aEvent.target;
    if (target.localName == "descriptionitem")
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
        var destFilePath = messenger.saveAttachmentToFolder(attachment.contentType, attachment.url, encodeURIComponent(attachment.displayName), attachment.uri, destDirectory);
        aData.value = destFilePath.QueryInterface(Components.interfaces.nsISupports);
        aDataLen.value = 4;
      }
    }
  }

}

function nsDummyMsgHeader()
{
}

nsDummyMsgHeader.prototype =
{
  mProperties : new Array,
  getStringProperty : function(property) {return this.mProperties[property];},
  setStringProperty : function(property, val) {this.mProperties[property] = val;},
  messageSize : 0,
  recipients : null,
  from : null,
  subject : null,
  ccList : null,
  messageId : null,
  accountKey : "",
  folder : null
};
