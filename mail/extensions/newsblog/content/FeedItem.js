// Hash of items being downloaded, indexed by URL, so the load event listener
// can access the FeedItem objects after it downloads their content.
// XXX Not currently being used, since we're not downloading content these days.
var gFzItemCache = new Object();

// Handy conversion values.
const HOURS_TO_MINUTES = 60;
const MINUTES_TO_SECONDS = 60;
const SECONDS_TO_MILLISECONDS = 1000;
const MINUTES_TO_MILLISECONDS = MINUTES_TO_SECONDS * SECONDS_TO_MILLISECONDS;
const HOURS_TO_MILLISECONDS = HOURS_TO_MINUTES * MINUTES_TO_MILLISECONDS;

const MSG_FLAG_NEW      = 0x10000;

function FeedItem() {
  // XXX Convert date to a consistent representation in a setter.
  this.date = new Date().toString();

  return this;
}

FeedItem.prototype.id = null;
FeedItem.prototype.feed = null;
FeedItem.prototype.description = null;
FeedItem.prototype.content = null;
FeedItem.prototype.title = "(no subject)";
FeedItem.prototype.author = "anonymous";

FeedItem.prototype._url = null;
FeedItem.prototype.url getter = function() { return this._url }
FeedItem.prototype.url setter = function(url) {
  var uri =
    Components
      .classes["@mozilla.org/network/standard-url;1"]
        .getService(Components.interfaces["nsIStandardURL"]);
  uri.init(1, 80, url, null, null);
  var uri = uri.QueryInterface(Components.interfaces.nsIURI);
  this._url = uri.spec;
}

// A string that identifies the item; currently only used in debug statements.
FeedItem.prototype.identity getter = function() { return this.feed.name + ": " + this.title + " (" + this.id + ")" }

FeedItem.prototype.messageID getter = function() {
  // XXX Make this conform to the message ID spec.

  var messageID = this.id || this.url || this.title;

  // Escape occurrences of message ID meta characters <, >, and @.
  messageID.replace(/</g, "%3C");
  messageID.replace(/>/g, "%3E");
  messageID.replace(/@/g, "%40");

  messageID = messageID + "@" + "localhost.localdomain";

  return messageID;
}

const MESSAGE_TEMPLATE = "\n\
<html>\n\
  <head>\n\
    <title>%TITLE%</title>\n\
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
    <iframe src=\"%URL%\">\n\
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

FeedItem.prototype.store = function() {
    if (this.isStored()) {
      debug(this.identity + " already stored; ignoring");
    }
    else if (this.content) {
      debug(this.identity + " has content; storing");
      var content = MESSAGE_TEMPLATE;
      content = content.replace(/%CONTENT_TEMPLATE%/, LOCAL_CONTENT_TEMPLATE);
      content = content.replace(/%STYLE%/, LOCAL_STYLE);
      content = content.replace(/%TITLE%/, this.title);
      content = content.replace(/%URL%/g, this.url);
      content = content.replace(/%CONTENT%/, this.content);
      this.content = content; // XXX store it elsewhere, f.e. this.page
      this.writeToFolder();
    }
    else if (this.feed.quickMode) {
      debug(this.identity + " in quick mode; storing");
      this.content = this.description || this.title;
      var content = MESSAGE_TEMPLATE;
      content = content.replace(/%CONTENT_TEMPLATE%/, LOCAL_CONTENT_TEMPLATE);
      content = content.replace(/%STYLE%/, LOCAL_STYLE);
      content = content.replace(/%TITLE%/, this.title);
      content = content.replace(/%URL%/g, this.url);
      content = content.replace(/%CONTENT%/, this.content);
      this.content = content; // XXX store it elsewhere, f.e. this.page
      this.writeToFolder();
    } else {
      //debug(this.identity + " needs content; downloading");
      debug(this.identity + " needs content; creating and storing");
      var content = MESSAGE_TEMPLATE;
      content = content.replace(/%CONTENT_TEMPLATE%/, REMOTE_CONTENT_TEMPLATE);
      content = content.replace(/%STYLE%/, REMOTE_STYLE);
      content = content.replace(/%TITLE%/, this.title);
      content = content.replace(/%URL%/g, this.url);
      content = content.replace(/%DESCRIPTION%/, this.description || this.title);
      this.content = content; // XXX store it elsewhere, f.e. this.page
      this.writeToFolder();
      //this.download();
    }
}

FeedItem.prototype.isStored = function() {
  // Checks to see if the item has already been stored in its feed's message folder.

  debug(this.identity + " checking to see if stored");

  var server = this.feed.server;
  var folder = this.feed.folder;

  try {
    if (!folder)
      folder = server.rootMsgFolder.getChildNamed(this.feed.name);
  } catch(e) {}

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
  var itemResource = rdf.GetResource(this.url || ("urn:" + this.id));
  var downloaded = ds.GetTarget(itemResource, FZ_STORED, true);
  if (!downloaded || downloaded.QueryInterface(Components.interfaces.nsIRDFLiteral).Value == "false") 
  {
    debug(this.identity + " not stored");
    return false;
  }
  else 
  {
    debug(this.identity + " stored");
    return true;
  }
}

// XXX This should happen in the constructor automatically.
FeedItem.prototype.markValid = function() {
    debug("validating " + this.url);

    var ds = getItemsDS(this.feed.server);
    var resource = rdf.GetResource(this.url || ("urn:" + this.id));
    
    if (!ds.HasAssertion(resource, FZ_FEED, rdf.GetResource(this.feed.url), true))
      ds.Assert(resource, FZ_FEED, rdf.GetResource(this.feed.url), true);
    
    if (ds.hasArcOut(resource, FZ_VALID)) {
      var currentValue = ds.GetTarget(resource, FZ_VALID, true);
      ds.Change(resource, FZ_VALID, currentValue, RDF_LITERAL_TRUE);
    }
    else {
      ds.Assert(resource, FZ_VALID, RDF_LITERAL_TRUE, true);
    }
}


FeedItem.prototype.markStored = function() {
    var ds = getItemsDS(this.feed.server);
    var resource = rdf.GetResource(this.url || ("urn:" + this.id));
    
    if (!ds.HasAssertion(resource, FZ_FEED, rdf.GetResource(this.feed.url), true))
      ds.Assert(resource, FZ_FEED, rdf.GetResource(this.feed.url), true);
    
    var currentValue;
    if (ds.hasArcOut(resource, FZ_STORED)) {
      currentValue = ds.GetTarget(resource, FZ_STORED, true);
      ds.Change(resource, FZ_STORED, currentValue, RDF_LITERAL_TRUE);
    }
    else {
      ds.Assert(resource, FZ_STORED, RDF_LITERAL_TRUE, true);
    }
}

FeedItem.prototype.download = function() {
  this.request = new XMLHttpRequest();
  this.request.item = this;
  this.request.open("GET", this.url);
  this.request.onload = FeedItem.onDownloaded;
  this.request.onerror = FeedItem.onDownloadError;
  this.request.send(null);
  //updateServerBusyState(1);
  gFzItemCache[this.url] = this;
}

FeedItem.onDownloaded = function(event) {
  var request = event.target;
  var url = request.channel.originalURI.spec;

  //updateServerBusyState(-1);

  var item = gFzItemCache[url];

  if (!item)
    throw("error after downloading " + url + ": couldn't retrieve item from request");
  if (!request.responseText)
    throw(item.identity + " content supposedly downloaded but missing");

  debug(item.identity + ": content downloaded");

  item.content = request.responseText;
  item.writeToFolder();
  delete gFzItemCache[url];
}

FeedItem.onDownloadError = function(event) {
  // XXX add error message if available and notify the user?
  var request = event.target;
  var url = request.channel.originalURI.spec;
  var item = gFzItemCache[url];
  throw("error downloading item " + (item ? item.identity : url));
}

FeedItem.unicodeConverter =
  Components
    .classes["@mozilla.org/intl/scriptableunicodeconverter"]
      .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
FeedItem.unicodeConverter.charset = "UTF-8";

FeedItem.prototype.toUtf8 = function(str) {
  return FeedItem.unicodeConverter.ConvertFromUnicode(str);
}

function mimeEncodeSubject(aSubject, charset)
{  
  // get the mime header encoder service
  var mimeEncoder = Components
    .classes["@mozilla.org/messenger/mimeconverter;1"]
    .getService(Components.interfaces.nsIMimeConverter);

  // this routine sometimes throws exceptions for mis encoded data so wrap it 
  // with a try catch for now..
  var newSubject;

  try {
    newSubject = mimeEncoder.encodeMimePartIIStr(aSubject, false, charset, 9, 72);
  }
  catch (ex) { 
    newSubject = aSubject; 
  }

  return newSubject;
}

FeedItem.prototype.writeToFolder = function() {
  debug(this.identity + " writing to message folder" + this.feed.name + "\n");
  
  var server = this.feed.server;

  // XXX Should we really be modifying the original data here instead of making
  // a copy of it?  Currently we never use the item object again after writing it
  // to the message folder, but will that always be the case?

  // If the sender isn't a valid email address, quote it so it looks nicer.
  if (this.author && this.author.indexOf('@') == -1)
    this.author = '<' + this.author + '>';

  // Compress white space in the subject to make it look better.
  this.title = this.title.replace(/[\t\r\n]+/g, " ");
  this.title = mimeEncodeSubject(this.title, this.characterSet);

  // If the date looks like it's in W3C-DTF format, convert it into
  // an IETF standard date.  Otherwise assume it's in IETF format.
  if (this.date.search(/^\d\d\d\d/) != -1)
    this.date = W3CToIETFDate(this.date);

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
  var openingLine = 'From - ' + this.date + '\n';

  var source =
    openingLine +
    'X-Mozilla-Status: 0000\n' +
    'X-Mozilla-Status2: 00000000\n' +
    'Date: ' + this.date + '\n' +
    'Message-Id: <' + this.messageID + '>\n' +
    'From: ' + this.author + '\n' +
    'MIME-Version: 1.0\n' +
    'Subject: ' + this.title + '\n' +
    'Content-Type: text/html; charset=' + this.characterSet + '\n' +
    'Content-Transfer-Encoding: 8bit\n' +
    'Content-Base: ' + this.url + '\n' +
    '\n' +
    this.content;
  debug(this.identity + " is " + source.length + " characters long");

  // Get the folder and database storing the feed's messages and headers.
  var folder = this.feed.folder ? this.feed.folder : server.rootMsgFolder.getChildNamed(this.feed.name);
  folder = folder.QueryInterface(Components.interfaces.nsIMsgLocalMailFolder);
  folder.addMessage(source);
  this.markStored();
}

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

