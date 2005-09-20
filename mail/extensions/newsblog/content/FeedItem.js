# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
# The Original Code is an RSS Feed Item
#
# The Initial Developer of the Original Code is
# The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2004
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
# ***** END LICENSE BLOCK ***** */

// Handy conversion values.
const HOURS_TO_MINUTES = 60;
const MINUTES_TO_SECONDS = 60;
const SECONDS_TO_MILLISECONDS = 1000;
const MINUTES_TO_MILLISECONDS = MINUTES_TO_SECONDS * SECONDS_TO_MILLISECONDS;
const HOURS_TO_MILLISECONDS = HOURS_TO_MINUTES * MINUTES_TO_MILLISECONDS;
const MSG_FLAG_NEW      = 0x10000;
const ENCLOSURE_BOUNDARY_PREFIX = "--------------";  // 14 dashes
const ENCLOSURE_HEADER_BOUNDARY_PREFIX = "------------"; // 12 dashes

const MESSAGE_TEMPLATE = "\n\
<html>\n\
  <head>\n\
    <title>%TITLE%</title>\n\
    <base href=\"%BASE%\">\n\
    <style type=\"text/css\">\n\
      %STYLE%\n\
    </style>\n\
  </head>\n\
  <body>\n\
    %CONTENT_TEMPLATE%\n\
  </body>\n\
</html>\n\
";

const REMOTE_CONTENT_TEMPLATE = "\n\
    <iframe id =\"_mailrssiframe\" src=\"%URL%\">\n\
      %DESCRIPTION%\n\
    </iframe>\n\
";

const REMOTE_STYLE = "\n\
      body {\n\
        margin: 0;\n\
        border: none;\n\
        padding: 0;\n\
      }\n\
      iframe {\n\
        position: fixed;\n\
        top: 0;\n\
        right: 0;\n\
        bottom: 0;\n\
        left: 0;\n\
        border: none;\n\
      }\n\
";

// Unlike remote content, which is locked within a fixed position iframe,
// local content goes is positioned according to the normal rules of flow.
// The problem with this is that the message pane itself provides a scrollbar
// if necessary, and that scrollbar appears next to the toolbar as well as
// the content being scrolled.  The solution is to lock local content within
// a fixed position div and set its overflow property to auto so that the div
// itself provides the scrollbar.  Unfortunately we can't do that because of
// Mozilla bug 97283, which makes it hard to scroll an auto overflow div.

const LOCAL_CONTENT_TEMPLATE = "\n\
      %CONTENT%\n\
";

// no local style overrides at this time
const LOCAL_STYLE = "\n";

function FeedItem() 
{
  this.mDate = new Date().toString();
  this.mUnicodeConverter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"]
                          .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
}

FeedItem.prototype = 
{
  isStoredWithId: false, // we currently only do this for IETF Atom. RSS2 with GUIDs should do this as well.
  xmlContentBase: null, // only for IETF Atom
  id: null,
  feed: null,
  description: null,
  content: null,
  enclosure: null,  // we currently only support one enclosure per feed item...
  title: "(no subject)",  // TO DO: this needs to be localized
  author: "anonymous",
  mURL: null,
  characterSet: "",

  get url()
  {
    return this.mURL;
  },

  set url(aVal)
  {
    var uri = Components.classes["@mozilla.org/network/standard-url;1"].getService(Components.interfaces["nsIStandardURL"]);
    uri.init(1, 80, aVal, null, null);
    var uri = uri.QueryInterface(Components.interfaces.nsIURI);
    this.mURL = uri.spec;
  },

  get date()
  {
    return this.mDate;
  },

  set date (aVal)
  {
    this.mDate = aVal;
  },

  get identity ()
  {
    return this.feed.name + ": " + this.title + " (" + this.id + ")"
  },

  get messageID()
  {
    var messageID = this.id || this.mURL || this.title;

    // Escape occurrences of message ID meta characters <, >, and @.
    messageID.replace(/</g, "%3C");
    messageID.replace(/>/g, "%3E");
    messageID.replace(/@/g, "%40");
    messageID = messageID + "@" + "localhost.localdomain";
    return messageID;
  },

  get itemUniqueURI()
  {
    var theURI;
    if(this.isStoredWithId && this.id)
      theURI = "urn:" + this.id;
    else
      theURI = this.mURL || ("urn:" + this.id);
    return theURI;
  },

  get contentBase()
  {
    if(this.xmlContentBase)
      return this.xmlContentBase
    else
      return this.mURL;
  },

  store: function() 
  {
    this.mUnicodeConverter.charset = this.characterSet;

    if (this.isStored()) 
      debug(this.identity + " already stored; ignoring");
    else if (this.content) 
    {
      debug(this.identity + " has content; storing");
      var content = MESSAGE_TEMPLATE;
      content = content.replace(/%CONTENT_TEMPLATE%/, LOCAL_CONTENT_TEMPLATE);
      content = content.replace(/%STYLE%/, LOCAL_STYLE);
      content = content.replace(/%TITLE%/, this.title);
      content = content.replace(/%BASE%/, this.contentBase); 
      content = content.replace(/%URL%/g, this.mURL);
      content = content.replace(/%CONTENT%/, this.content);
      this.content = content; // XXX store it elsewhere, f.e. this.page
      this.writeToFolder();
    }
    else if (this.feed.quickMode) 
    {
      debug(this.identity + " in quick mode; storing");

      this.content = this.description || this.title;

      var content = MESSAGE_TEMPLATE;
      content = content.replace(/%CONTENT_TEMPLATE%/, LOCAL_CONTENT_TEMPLATE);
      content = content.replace(/%STYLE%/, LOCAL_STYLE);
      content = content.replace(/%BASE%/, this.contentBase); 
      content = content.replace(/%TITLE%/, this.title);
      content = content.replace(/%URL%/g, this.mURL);
      content = content.replace(/%CONTENT%/, this.content);
      this.content = content; // XXX store it elsewhere, f.e. this.page
      this.writeToFolder();
    } 
    else 
    {
      //debug(this.identity + " needs content; downloading");
      debug(this.identity + " needs content; creating and storing");
      var content = MESSAGE_TEMPLATE;
      content = content.replace(/%CONTENT_TEMPLATE%/, REMOTE_CONTENT_TEMPLATE);
      content = content.replace(/%STYLE%/, REMOTE_STYLE);
      content = content.replace(/%TITLE%/, this.title);
      content = content.replace(/%BASE%/, this.contentBase); 
      content = content.replace(/%URL%/g, this.mURL);
      content = content.replace(/%DESCRIPTION%/, this.description || this.title);
      this.content = content; // XXX store it elsewhere, f.e. this.page
      this.writeToFolder();
    }
  },

  isStored: function()
  {
    // Checks to see if the item has already been stored in its feed's message folder.

    debug(this.identity + " checking to see if stored");

    var server = this.feed.server;
    var folder = this.feed.folder;

    if (!folder) 
    {
      debug(this.feed.name + " folder doesn't exist; creating");
		  debug("creating " + this.feed.name + "as child of " + server.rootMsgFolder + "\n");
      server.rootMsgFolder.createSubfolder(this.feed.name, null /* supposed to be a msg window */);
      folder = server.rootMsgFolder.FindSubFolder(this.feed.name);
      debug(this.identity + " not stored (folder didn't exist)");
      return false;
    }

    var ds = getItemsDS(server);
    var itemURI = this.itemUniqueURI;
    var itemResource = rdf.GetResource(itemURI);

    var downloaded = ds.GetTarget(itemResource, FZ_STORED, true);
    if (!downloaded || downloaded.QueryInterface(Components.interfaces.nsIRDFLiteral).Value == "false") 
    {
      // HACK ALERT: before we give up, try to work around an entity escaping bug in RDF
      // See Bug #258465 for more details
      itemURI = itemURI.replace(/&lt;/g, '<');
      itemURI = itemURI.replace(/&gt;/g, '>');
      itemURI = itemURI.replace(/&quot;/g, '"');
      itemURI = itemURI.replace(/&amp;/g, '&');     

      debug('Failed to find item, trying entity replacement version: '  + itemURI);
      itemResource = rdf.GetResource(itemURI);
      downloaded = ds.GetTarget(itemResource, FZ_STORED, true);

      if (downloaded)
      { 
        debug(this.identity + " not stored");
        return true;
      }

      debug(this.identity + " not stored");
      return false;
    }
    else 
    {
      debug(this.identity + " stored");
      return true;
    }
  },

  markValid: function() 
  {
    debug("validating " + this.mURL);
    var ds = getItemsDS(this.feed.server);

    var itemURI = this.itemUniqueURI;
    var resource = rdf.GetResource(itemURI);
    
    if (!ds.HasAssertion(resource, FZ_FEED, rdf.GetResource(this.feed.url), true))
      ds.Assert(resource, FZ_FEED, rdf.GetResource(this.feed.url), true);
    
    if (ds.hasArcOut(resource, FZ_VALID)) 
    {
      var currentValue = ds.GetTarget(resource, FZ_VALID, true);
      ds.Change(resource, FZ_VALID, currentValue, RDF_LITERAL_TRUE);
    }
    else 
      ds.Assert(resource, FZ_VALID, RDF_LITERAL_TRUE, true);
  },

  markStored: function() 
  {
    var ds = getItemsDS(this.feed.server);
    var itemURI = this.itemUniqueURI;
    var resource = rdf.GetResource(itemURI);
   
    if (!ds.HasAssertion(resource, FZ_FEED, rdf.GetResource(this.feed.url), true))
      ds.Assert(resource, FZ_FEED, rdf.GetResource(this.feed.url), true);
    
    var currentValue;
    if (ds.hasArcOut(resource, FZ_STORED)) 
    {
      currentValue = ds.GetTarget(resource, FZ_STORED, true);
      ds.Change(resource, FZ_STORED, currentValue, RDF_LITERAL_TRUE);
    }
    else 
      ds.Assert(resource, FZ_STORED, RDF_LITERAL_TRUE, true);
  },

  mimeEncodeSubject: function(aSubject, aCharset)
  {  
    // get the mime header encoder service
    var mimeEncoder = Components.classes["@mozilla.org/messenger/mimeconverter;1"].getService(Components.interfaces.nsIMimeConverter);

    // this routine sometimes throws exceptions for mis-encoded data so wrap it 
    // with a try catch for now..
    var newSubject;
    try 
    {
      newSubject = mimeEncoder.encodeMimePartIIStr(this.mUnicodeConverter.ConvertFromUnicode(aSubject), false, aCharset, 9, 72);
    }
    catch (ex) 
    { 
      newSubject = aSubject; 
    }

    return newSubject;
  }, 

  writeToFolder: function() 
  {
    debug(this.identity + " writing to message folder" + this.feed.name + "\n");
  
    var server = this.feed.server;
    this.mUnicodeConverter.charset = this.characterSet;

    // If the sender isn't a valid email address, quote it so it looks nicer.
    if (this.author && this.author.indexOf('@') == -1)
      this.author = '<' + this.author + '>';

    // Convert the title to UTF-16 before performing our HTML entity replacement
    // reg expressions.
    var title = this.title; 

    // the subject may contain HTML entities.
    // Convert these to their unencoded state. i.e. &amp; becomes '&'
    title = title.replace(/&lt;/g, '<');
    title = title.replace(/&gt;/g, '>');
    title = title.replace(/&quot;/g, '"');
    title = title.replace(/&amp;/g, '&');
  
    // Compress white space in the subject to make it look better.
    title = title.replace(/[\t\r\n]+/g, " ");

    this.title = this.mimeEncodeSubject(title, this.characterSet);

    // If the date looks like it's in W3C-DTF format, convert it into
    // an IETF standard date.  Otherwise assume it's in IETF format.
    if (this.mDate.search(/^\d\d\d\d/) != -1)
      this.mDate = W3CToIETFDate(this.mDate);

    // Escape occurrences of "From " at the beginning of lines of content
    // per the mbox standard, since "From " denotes a new message, and add
    // a line break so we know the last line has one.
    this.content = this.content.replace(/([\r\n]+)(>*From )/g, "$1>$2");
    this.content += "\n";

    // The opening line of the message, mandated by standards to start with
    // "From ".  It's useful to construct this separately because we not only
    // need to write it into the message, we also need to use it to calculate
    // the offset of the X-Mozilla-Status lines from the front of the message
    // for the statusOffset property of the DB header object.
    var openingLine = 'From - ' + this.mDate + '\n';

    var source =
      openingLine +
      'X-Mozilla-Status: 0000\n' +
      'X-Mozilla-Status2: 00000000\n' +
      'Date: ' + this.mDate + '\n' +
      'Message-Id: <' + this.messageID + '>\n' +
      'From: ' + this.author + '\n' +
      'MIME-Version: 1.0\n' +
      'Subject: ' + this.title + '\n' +
      'Content-Transfer-Encoding: 8bit\n' +
      'Content-Base: ' + this.mURL + '\n';

    if (this.enclosure && this.enclosure.mFileName)
    {
      var boundaryID = source.length + this.enclosure.mLength;
      source += 'Content-Type: multipart/mixed;\n boundary="' + ENCLOSURE_HEADER_BOUNDARY_PREFIX + boundaryID + '"' + '\n\n' +
                'This is a multi-part message in MIME format.\n' + ENCLOSURE_BOUNDARY_PREFIX + boundaryID + '\n' +
                'Content-Type: text/html; charset=' + this.characterSet + '\n' +
                'Content-Transfer-Encoding: 8bit\n' + 
                this.content;
      source += this.enclosure.convertToAttachment(boundaryID);
    }
    else
    {
      source += 'Content-Type: text/html; charset=' + this.characterSet + '\n' +
                '\n' + this.content;
               
    }

    debug(this.identity + " is " + source.length + " characters long");

    // Get the folder and database storing the feed's messages and headers.
    folder = this.feed.folder.QueryInterface(Components.interfaces.nsIMsgLocalMailFolder);

    // source is a unicode string, we want to save a char * string in the original charset. So convert back
    folder.addMessage(this.mUnicodeConverter.ConvertFromUnicode(source));
    this.markStored();
  }
};


// A feed enclosure is to RSS what an attachment is for e-mail. We make enclosures look
// like attachments in the UI.

function FeedEnclosure(aURL, aContentType, aLength) 
{
  this.mURL = aURL;
  this.mContentType = aContentType;
  this.mLength = aLength;

  // generate a fileName from the URL
  if (this.mURL)
  {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    var enclosureURL  = ioService.newURI(this.mURL, null, null).QueryInterface(Components.interfaces.nsIURL);
    if (enclosureURL)
      this.mFileName = enclosureURL.fileName;
  }
}

FeedEnclosure.prototype = 
{
  mURL: "",
  mContentType: "",
  mLength: 0,
  mFileName: "",

  // returns a string that looks like an e-mail attachment
  // which represents the enclosure.
  convertToAttachment: function(aBoundaryID)
  {
    return '\n' +
                  ENCLOSURE_BOUNDARY_PREFIX + aBoundaryID + '\n' +
                  'Content-Type: ' + this.mContentType + '; name="' + this.mFileName + '"\n' + 
                  'X-Mozilla-External-Attachment-URL: ' + this.mURL + '\n' +
                  'Content-Disposition: attachment; filename="' + this.mFileName + '"\n\n' + 
                  'This MIME attachment is stored separately from the message.\n' +
                  ENCLOSURE_BOUNDARY_PREFIX + aBoundaryID + '--' + '\n';

  }
};

function W3CToIETFDate(dateString) {
  // Converts a W3C-DTF (subset of ISO 8601) date string to an IETF date string.
  // W3C-DTF is described in this note: http://www.w3.org/TR/NOTE-datetime
  // IETF is obtained via the Date object's toUTCString() method.  The object's
  // toString() method is insufficient because it spells out timezones on Win32
  // (f.e. "Pacific Standard Time" instead of "PST"), which Mail doesn't grok.
  // For info, see http://lxr.mozilla.org/mozilla/source/js/src/jsdate.c#1526.

  var parts = dateString.match(/(\d\d\d\d)(-(\d\d))?(-(\d\d))?(T(\d\d):(\d\d)(:(\d\d)(\.(\d+))?)?(Z|([+-])(\d\d):(\d\d))?)?/);
  debug("date parts: " + parts);

  // Here's an example of a W3C-DTF date string and what .match returns for it.
  //  date: 2003-05-30T11:18:50.345-08:00
  //  date.match returns array values:
  //   0: 2003-05-30T11:18:50-08:00,
  //   1: 2003,
  //   2: -05,
  //   3: 05,
  //   4: -30,
  //   5: 30,
  //   6: T11:18:50-08:00,
  //   7: 11,
  //   8: 18,
  //   9: :50,
  //   10: 50,
  //   11: .345,
  //   12: 345,
  //   13: -08:00,
  //   14: -,
  //   15: 08,
  //   16: 00

  // Create a Date object from the date parts.  Note that the Date object
  // apparently can't deal with empty string parameters in lieu of numbers,
  // so optional values (like hours, minutes, seconds, and milliseconds)
  // must be forced to be numbers.
  var date = new Date(parts[1], parts[3]-1, parts[5], parts[7] || 0,
                      parts[8] || 0, parts[10] || 0, parts[12] || 0);

  // We now have a value that the Date object thinks is in the local timezone
  // but which actually represents the date/time in the remote timezone
  // (f.e. the value was "10:00 EST", and we have converted it to "10:00 PST"
  // instead of "07:00 PST").  We need to correct that.  To do so, we're going
  // to add the offset between the remote timezone and UTC (to convert the value
  // to UTC), then add the offset between UTC and the local timezone (to convert
  // the value to the local timezone).

  // Ironically, W3C-DTF gives us the offset between UTC and the remote timezone
  // rather than the other way around, while the getTimezoneOffset() method
  // of a Date object gives us the offset between the local timezone and UTC
  // rather than the other way around.  Both of these are the additive inverse
  // (i.e. -x for x) of what we want, so we have to invert them to use them
  // by multipying by -1
  // (f.e. if "the offset between UTC and the remote timezone" is -5 hours,
  // then "the offset between the remote timezone and UTC" is -5*-1 = 5 hours).

  // Note that if the timezone portion of the date/time string is absent
  // (which violates W3C-DTF, although ISO 8601 allows it), we assume the value
  // to be in UTC.

  // The offset between the remote timezone and UTC in milliseconds.
  var remoteToUTCOffset = 0;
  if (parts[13] && parts[13] != "Z") {
    var direction = (parts[14] == "+" ? 1 : -1);
    if (parts[15])
      remoteToUTCOffset += direction * parts[15] * HOURS_TO_MILLISECONDS;
    if (parts[16])
      remoteToUTCOffset += direction * parts[16] * MINUTES_TO_MILLISECONDS;
  }
  remoteToUTCOffset = remoteToUTCOffset * -1; // invert it
  debug("date remote to UTC offset: " + remoteToUTCOffset);

  // The offset between UTC and the local timezone in milliseconds.
  var UTCToLocalOffset = date.getTimezoneOffset() * MINUTES_TO_MILLISECONDS;
  UTCToLocalOffset = UTCToLocalOffset * -1; // invert it
  debug("date UTC to local offset: " + UTCToLocalOffset);

  date.setTime(date.getTime() + remoteToUTCOffset + UTCToLocalOffset);

  debug("date string: " + date.toUTCString());

  return date.toUTCString();
}
