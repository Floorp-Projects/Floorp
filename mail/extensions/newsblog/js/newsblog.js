/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the News&Blog Feed Downloader
 *
 *
 * Contributor(s):
 *  Myk Melez <myk@mozilla.org) (Original Author)
 *  David Bienvenu <bienvenu@nventure.com> 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

const kDebug               = true;

var nsNewsBlogFeedDownloader =
{
  downloadFeed: function(aUrl, aFolder, aQuickMode, aTitle, aUrlListener, aMsgWindow)
  {
    // we might just pull all these args out of the aFolder DB, instead of passing them in...
    feed = new Feed(aUrl, aQuickMode, aTitle);
    feed.urlListener = aUrlListener;
		feed.folder = aFolder;
		feed.msgWindow = aMsgWindow;
    feed.download();
  },
 
  QueryInterface: function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsINewsBlogFeedDownloader) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  }
}

var nsNewsBlogFeedDownloaderModule =
{
  mClassName:     "News+Blog Feed Downloader",
  mContractID:    "@mozilla.org/newsblog-feed-downloader;1",
  mClassID:       Components.ID("5c124537-adca-4456-b2b5-641ab687d1f6"),

  getClassObject: function(aCompMgr, aCID, aIID)
  {
    if (!aCID.equals(this.mClassID))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    if (!aIID.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    return this.mFactory;
  },

  registerSelf: function(aCompMgr, aFileSpec, aLocation, aType)
  {
    if (kDebug)
      dump("*** Registering nsNewsBlogFeedDownloaderModule (a JavaScript Module)\n");

    aCompMgr = aCompMgr.QueryInterface(
                 Components.interfaces.nsIComponentRegistrar);
    aCompMgr.registerFactoryLocation(this.mClassID, this.mClassName,
      this.mContractID, aFileSpec, aLocation, aType);

  },

  unregisterSelf: function(aCompMgr, aFileSpec, aLocation)
  {
    aCompMgr = aCompMgr.QueryInterface(
                 Components.interfaces.nsIComponentRegistrar);
    aCompMgr.unregisterFactoryLocation(this.mClassID, aFileSpec);

  },

  canUnload: function(aCompMgr)
  {
    return true;
  },

  //////////////////////////////////////////////////////////////////////
  //
  //   mFactory : nsIFactory
  //
  //////////////////////////////////////////////////////////////////////
  mFactory:
  {
    createInstance: function(aOuter, aIID)
    {
      if (aOuter != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      if (!aIID.equals(Components.interfaces.nsINewsBlogFeedDownloader) &&
          !aIID.equals(Components.interfaces.nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

      // return the singleton
      return nsNewsBlogFeedDownloader.QueryInterface(aIID);
    },

    lockFactory: function(aLock)
    {
    }
  }
};

function NSGetModule(aCompMgr, aFileSpec)
{
  return nsNewsBlogFeedDownloaderModule;
}

////////////////////////////////////////////////////////////////////////
//
//   Debug helper
//
////////////////////////////////////////////////////////////////////////
if (!kDebug)
  debug = function(m) {};
else
  debug = function(m) {dump("\t *** nsNewsBlogFeedDownloader: " + m + "\n");};



//---------------------------------------------------------
// From Feed.js

var rdfcontainer =
  Components
    .classes["@mozilla.org/rdf/container-utils;1"]
      .getService(Components.interfaces.nsIRDFContainerUtils);

var rdfparser =
  Components
    .classes["@mozilla.org/rdf/xml-parser;1"]
      .createInstance(Components.interfaces.nsIRDFXMLParser);

// For use when serializing content in Atom feeds.
var serializer = Components
    .classes["@mozilla.org/xmlextras/xmlserializer;1"]
      .createInstance(Components.interfaces.nsIDOMSerializer);

//new XMLSerializer;

// Hash of feeds being downloaded, indexed by URL, so the load event listener
// can access the Feed objects after it finishes downloading the feed files.
var gFzFeedCache = new Object();

function Feed(url, quickMode, title) {
  this.url = url;
  this.quickMode = quickMode || false;
  this.title = title || null;

  this.description = null;
  this.author = null;

  this.request = null;

  return this;
}

// The name of the message folder corresponding to the feed.
// XXX This should be called something more descriptive like "folderName".
// XXX Or maybe, when we support nested folders and downloading into any folder,
// there could just be a reference to the folder itself called "folder".
Feed.prototype.name getter = function() {
  var name = this.title || this.description || this.url;
  if (!name)
    throw("couldn't compute feed name, as feed has no title, description, or URL.");

  // Make sure the feed name doesn't have any line breaks, since we're going
  // to use it as the name of the folder in the filesystem.  This may not
  // be necessary, since Mozilla's mail code seems to handle other forbidden
  // characters in filenames and can probably handle these as well.
  name = name.replace(/[\n\r\t]+/g, " ");

  // Make sure the feed doesn't end in a period to work around bug 117840.
  name = name.replace(/\.+$/, "");

  return name;
}

Feed.prototype.download = function(async, parseItems) {
  // Whether or not to download the feed asynchronously.
  async = async == null ? true : async ? true : false;

  // Whether or not to parse items when downloading and parsing the feed.
  // Defaults to true, but setting to false is useful for obtaining
  // just the title of the feed when the user subscribes to it.
  this.parseItems = parseItems == null ? true : parseItems ? true : false;

  this.request =   Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
      .createInstance(Components.interfaces.nsIXMLHttpRequest);

  this.request.open("GET", this.url, async);
  this.request.overrideMimeType("text/xml");
  if (async) {
    this.request.onload = Feed.onDownloaded;
    this.request.onerror = Feed.onDownloadError;
    gFzFeedCache[this.url] = this;
  }
  this.request.send(null);
  if (!async) {
    this.parse();
  }
}

Feed.onDownloaded = function(event) {
  var request = event.target;
  var url = request.channel.originalURI.spec;
  debug(url + " downloaded");
  var feed = gFzFeedCache[url];
  if (!feed)
    throw("error after downloading " + url + ": couldn't retrieve feed from request");
  feed.parse();
}

Feed.onDownloadError = function(event) {
  // XXX add error message if available and notify the user?
  var request = event.target;
  var url = request.channel.originalURI.spec;
  var feed = gFzFeedCache[url];
  if (feed)
    debug(feed.title + " download failed");
  throw("error downloading feed " + url);
}

Feed.prototype.parse = function() {
  // Figures out what description language (RSS, Atom) and version this feed
  // is using and calls a language/version-specific feed parser.

  debug("parsing feed " + this.url);

  if (!this.request.responseText) {
    throw("error parsing feed " + this.url + ": no data");
    return;
  }
  else if (this.request.responseText.search(/="http:\/\/purl\.org\/rss\/1\.0\/"/) != -1) {
    debug(this.url + " is an RSS 1.x (RDF-based) feed");
    this.parseAsRSS1();
  }
  else if (this.request.responseText.search(/="http:\/\/purl.org\/atom\/ns#"/) != -1) {
    debug(this.url + " is an Atom feed");
    this.parseAsAtom();
  }
  else if (this.request.responseText.search(/"http:\/\/my\.netscape\.com\/rdf\/simple\/0\.9\/"/) != -1)
  {
    // RSS 0.9x is forward compatible with RSS 2.0, so use the RSS2 parser to handle it.
    debug(this.url + " is an 0.9x feed");
    this.parseAsRSS2();
  }
  // XXX Explicitly check for RSS 2.0 instead of letting it be handled by the
  // default behavior (who knows, we may change the default at some point).
  else {
    // We don't know what kind of feed this is; let's pretend it's RSS 0.9x
    // and hope things work out for the best.  In theory even RSS 1.0 feeds
    // could be parsed by the 0.9x parser if the RSS namespace was the default.
    debug(this.url + " is of unknown format; assuming an RSS 0.9x feed");
    this.parseAsRSS2();
  }
}

Feed.prototype.parseAsRSS2 = function() {
  if (!this.request.responseXML || !(this.request.responseXML instanceof Components.interfaces.nsIDOMXMLDocument))
    throw("error parsing RSS 2.0 feed " + this.url + ": data not parsed into XMLDocument object");

  // Get the first channel (assuming there is only one per RSS File).
  var channel = this.request.responseXML.getElementsByTagName("channel")[0];
  if (!channel)
    throw("error parsing RSS 2.0 feed " + this.url + ": channel element missing");

  this.title = this.title || getNodeValue(channel.getElementsByTagName("title")[0]);
  this.description = getNodeValue(channel.getElementsByTagName("description")[0]);

  if (!this.parseItems)
    return;

  var itemNodes = this.request.responseXML.getElementsByTagName("item");
  for ( var i=0 ; i<itemNodes.length ; i++ ) {
    var itemNode = itemNodes[i];
    var item = new FeedItem();
    item.feed = this;

    var link = getNodeValue(itemNode.getElementsByTagName("link")[0]);

    var guidNode = itemNode.getElementsByTagName("guid")[0];
    if (guidNode) {
      var guid = getNodeValue(guidNode);
      var isPermaLink =
        guidNode.getAttribute('isPermaLink') == 'false' ? false : true;
    }

    item.url = (guid && isPermaLink) ? guid : link ? link : null;
    item.id = guid;
    item.description = getNodeValue(itemNode.getElementsByTagName("description")[0]);
    item.title = getNodeValue(itemNode.getElementsByTagName("title")[0])
                 || (item.description ? item.description.substr(0, 150) : null)
                 || item.title;
    item.author = getNodeValue(itemNode.getElementsByTagName("author")[0]
                               || itemNode.getElementsByTagName("creator")[0]
                               || channel.getElementsByTagName("creator")[0])
                  || this.title
                  || item.author;
    item.date = getNodeValue(itemNode.getElementsByTagName("pubDate")[0]
                             || itemNode.getElementsByTagName("date")[0])
                || item.date;

    item.store();
  }
}

Feed.prototype.parseAsRSS1 = function() {
  // RSS 1.0 is valid RDF, so use the RDF parser/service to extract data.

  // Create a new RDF data source and parse the feed into it.
  var ds = Components
             .classes["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"]
               .createInstance(Components.interfaces.nsIRDFDataSource);
  rdfparser.parseString(ds, this.request.channel.URI, this.request.responseText);

  // Get information about the feed as a whole.
  var channel = ds.GetSource(RDF_TYPE, RSS_CHANNEL, true);
  this.title = this.title || getRDFTargetValue(ds, channel, RSS_TITLE);
  this.description = getRDFTargetValue(ds, channel, RSS_DESCRIPTION);

  if (!this.parseItems)
    return;

  var items = ds.GetTarget(channel, RSS_ITEMS, true);
  //items = items.QueryInterface(Components.interfaces.nsIRDFContainer);
  items = rdfcontainer.MakeSeq(ds, items);
  items = items.GetElements();
  // If the channel doesn't list any items, look for resources of type "item"
  // (a hacky workaround for some buggy feeds).
  if (!items.hasMoreElements())
    items = ds.GetSources(RDF_TYPE, RSS_ITEM, true);

  while (items.hasMoreElements()) {
    var itemResource = items.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    var item = new FeedItem();
    item.feed = this;

    // Prefer the value of the link tag to the item URI since the URI could be
    // a relative URN.
    var uri = itemResource.Value;
    var link = getRDFTargetValue(ds, itemResource, RSS_LINK);

    item.url = link || uri;
    item.id = item.url;
    item.description = getRDFTargetValue(ds, itemResource, RSS_DESCRIPTION);
    item.title = getRDFTargetValue(ds, itemResource, RSS_TITLE)
                 || getRDFTargetValue(ds, itemResource, DC_SUBJECT)
                 || (item.description ? item.description.substr(0, 150) : null)
                 || item.title;
    item.author = getRDFTargetValue(ds, itemResource, DC_CREATOR)
                  || getRDFTargetValue(ds, channel, DC_CREATOR)
                  || this.title
                  || item.author;
    item.date = getRDFTargetValue(ds, itemResource, DC_DATE) || item.date;
    item.content = getRDFTargetValue(ds, itemResource, RSS_CONTENT_ENCODED);

    item.store();
  }
}

Feed.prototype.parseAsAtom = function() {
  if (!this.request.responseXML || !(this.request.responseXML instanceof Components.interfaces.nsIDOMXMLDocument))
    throw("error parsing Atom feed " + this.url + ": data not parsed into XMLDocument object");

  // Get the first channel (assuming there is only one per Atom File).
  var channel = this.request.responseXML.getElementsByTagName("feed")[0];
  if (!channel)
    throw("channel missing from Atom feed " + request.channel.name);

  this.title = this.title || getNodeValue(channel.getElementsByTagName("title")[0]);
  this.description = getNodeValue(channel.getElementsByTagName("tagline")[0]);

  if (!this.parseItems)
    return;

  var items = this.request.responseXML.getElementsByTagName("entry");
  for ( var i=0 ; i<items.length ; i++ ) {
    var itemNode = items[i];
    var item = new FeedItem();
    item.feed = this;

    var url;
    var links = itemNode.getElementsByTagName("link");
    for ( var j=0 ; j<links.length ; j++ ) {
      var alink = links[j];
      if (alink && alink.getAttribute('rel') && alink.getAttribute('rel') == 'alternate' && alink.getAttribute('href')) {
        url = alink.getAttribute('href');
        break;
      }
    }

    item.url = url;
    item.id = getNodeValue(itemNode.getElementsByTagName("id")[0]);
    item.description = getNodeValue(itemNode.getElementsByTagName("summary")[0]);
    item.title = getNodeValue(itemNode.getElementsByTagName("title")[0])
                 || (item.description ? item.description.substr(0, 150) : null)
                 || item.title;

    var author = itemNode.getElementsByTagName("author")[0]
                 || itemNode.getElementsByTagName("contributor")[0]
                 || channel.getElementsByTagName("author")[0];
    if (author) {
      var name = getNodeValue(author.getElementsByTagName("name")[0]);
      var email = getNodeValue(author.getElementsByTagName("email")[0]);
      if (name)
        author = name + (email ? " <" + email + ">" : "");
      else if (email)
        author = email;
    }
    item.author = author || item.author || this.title;

    item.date = getNodeValue(itemNode.getElementsByTagName("modified")[0]
                             || itemNode.getElementsByTagName("issued")[0]
                             || itemNode.getElementsByTagName("created")[0])
                || item.date;

    // XXX We should get the xml:base attribute from the content tag as well
    // and use it as the base HREF of the message.
    // XXX Atom feeds can have multiple content elements; we should differentiate
    // between them and pick the best one.
    // Some Atom feeds wrap the content in a CTYPE declaration; others use
    // a namespace to identify the tags as HTML; and a few are buggy and put
    // HTML tags in without declaring their namespace so they look like Atom.
    // We deal with the first two but not the third.
    var content;
    var contentNode = itemNode.getElementsByTagName("content")[0];
    if (contentNode) {
      content = "";
      for ( var j=0 ; j<contentNode.childNodes.length ; j++ ) {
        var node = contentNode.childNodes.item(j);
        if (node.nodeType == node.CDATA_SECTION_NODE)
          content += node.data;
        else
          content += serializer.serializeToString(node);
          //content += getNodeValue(node);
      }
      if (contentNode.getAttribute('mode') == "escaped") {
        content = content.replace(/&lt;/g, "<");
        content = content.replace(/&gt;/g, ">");
        content = content.replace(/&amp;/g, "&");
      }
      if (content == "")
        content = null;
    }
    item.content = content;

    item.store();
  }
}


// -----------------------------------------------
// From FeedItem.js
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
      body {\n\
        margin: 0;\n\
        border: none;\n\
        padding: 0;\n\
      }\n\
      #toolbar {\n\
        position: fixed;\n\
        top: 0;\n\
        right: 0;\n\
        left: 0;\n\
        height: 1.4em;\n\
        margin: 0;\n\
        border-bottom: thin solid black;\n\
        padding-left: 0.5em;\n\
        background-color: -moz-dialog;\n\
      }\n\
      %STYLE%\n\
    </style>\n\
  </head>\n\
  <body>\n\
    <div id=\"toolbar\">\n\
      <a href=\"%URL%\">view in browser</a>\n\
    </div>\n\
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
      iframe {\n\
        position: fixed;\n\
        top: 1.4em;\n\
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
    <div id=\"content\">\n\
      %CONTENT%\n\
    </div>\n\
";

// We pad the top more to account for the space taken up by the toolbar.
// In theory the top should be padded 1.8em if we want 0.4em of padding between
// the 1.4em high toolbar and the content, but extra padding seems to get added
// to the top of the message, so we have to reduce the top padding accordingly.

const LOCAL_STYLE = "\n\
      #content {\n\
        margin: 0;\n\
        border: none;\n\
        padding: 0.4em;\n\
        padding-top: 1.8em;\n\
      }\n\
";

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

  var server = this.feed.folder.server;
  var folder;
  try {
    //var folder = server.rootMsgFolder.FindSubFolder(feed);
    folder = server.rootMsgFolder.getChildNamed(this.feed.name);
  } catch(e) {
    folder = null;
  }
  if (!folder) {
    debug(this.feed.name + " folder doesn't exist; creating");
    server.rootMsgFolder.createSubfolder(this.feed.name, this.feed.msgWindow);
    debug(this.identity + " not stored (folder didn't exist)");
    return false;
  }

  try {
    folder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
    var db = folder.getMsgDatabase(this.feed.msgWindow);
    var hdr = db.getMsgHdrForMessageID(this.messageID);
    if (hdr) {
      debug(this.identity + " stored");
      return true;
    }
    else {
      debug(this.identity + " not stored? let's check all headers");
      var foo = db.EnumerateMessages();
      var i=0;
      while (foo.hasMoreElements()) {
        ++i;
        var bar = foo.getNext();
        bar = bar.QueryInterface(Components.interfaces.nsIMsgDBHdr);
        if (this.messageID == bar.messageId) {
          debug(this.identity + " stored (found it while checking all headers)");
          return true;
        }
      }
      debug(this.identity + " not stored (checked " + i + " headers but couldn't find it)");
      return false;
    }
  }
  catch(e) {
    debug(this.identity + " error checking if stored: " + e);
    throw(e);
  }
}

FeedItem.prototype.download = function() {
  this.request = 
  Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
      .createInstance(Components.interfaces.nsIXMLHttpRequest);
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

FeedItem.prototype.writeToFolder = function() {
  debug(this.identity + " writing to message folder");

  var server = this.feed.folder.server;

  // XXX Should we really be modifying the original data here instead of making
  // a copy of it?  Currently we never use the item object again after writing it
  // to the message folder, but will that always be the case?

  // If the sender isn't a valid email address, quote it so it looks nicer.
  if (this.author && this.author.indexOf('@') == -1)
    this.author = '<' + this.author + '>';

  // Compress white space in the subject to make it look better.
  this.title = this.title.replace(/[\t\r\n]+/g, " ");

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
    'Content-Type: text/html; charset=UTF-8\n' +
    'Content-Transfer-Encoding: 8bit\n' +
    'Content-Base: ' + this.url + '\n' +
    '\n' +
    this.content;
  debug(this.identity + " is " + source.length + " characters long");

  // Get the folder and database storing the feed's messages and headers.
  var folder = server.rootMsgFolder.getChildNamed(this.feed.name);
  folder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);

  var mbox = new LocalFile(folder.path.nativePath,
                           MODE_WRONLY | MODE_APPEND | MODE_CREATE);

  // Create a message header object using the offset of the message
  // from the front of the mbox file as the unique message key (which seems
  // to be what other mail code does).
  var key = mbox.localFile.fileSize;

  var length = mbox.write(this.toUtf8(source));
  mbox.flush();
  mbox.close();

  var db = folder.getMsgDatabase(this.feed.msgWindow);
  var header = db.CreateNewHdr(key);

  // Not sure what date should be, but guessing it's seconds-since-epoch
  // since that's what existing values look like.  For some reason the values
  // in the database have three more decimal places than the values produced
  // by Date.getTime(), and in the DB values I've inspected they were all zero,
  // so I append three zeros here, which seems to work.  Sheesh what a hack.
  // XXX Figure out what's up with that, maybe milliseconds?
  header.date = new Date(this.date).getTime() + "000";
  header.Charset = "UTF-8";
  header.author = this.toUtf8(this.author);
  header.subject = this.toUtf8(this.title);
  header.messageId = this.messageID;
  header.messageSize = length;
  // Count the number of line break characters to determine the line count.
  header.lineCount = this.content.match(/\r?\n?/g).length;
  header.messageOffset = key;
  header.statusOffset = openingLine.length;

  db.AddNewHdrToDB(header, true);
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

// -------------------------------------------------
// From File-utils.js

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is The JavaScript Debugger
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Robert Ginda, <rginda@netscape.com>, original author
 *
 */

/* notice that these valuse are octal. */
const PERM_IRWXU = 00700;  /* read, write, execute/search by owner */
const PERM_IRUSR = 00400;  /* read permission, owner */
const PERM_IWUSR = 00200;  /* write permission, owner */
const PERM_IXUSR = 00100;  /* execute/search permission, owner */
const PERM_IRWXG = 00070;  /* read, write, execute/search by group */
const PERM_IRGRP = 00040;  /* read permission, group */
const PERM_IWGRP = 00020;  /* write permission, group */
const PERM_IXGRP = 00010;  /* execute/search permission, group */
const PERM_IRWXO = 00007;  /* read, write, execute/search by others */
const PERM_IROTH = 00004;  /* read permission, others */
const PERM_IWOTH = 00002;  /* write permission, others */
const PERM_IXOTH = 00001;  /* execute/search permission, others */

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_RDWR     = 0x04;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;
const MODE_SYNC     = 0x40;
const MODE_EXCL     = 0x80;

const PICK_OK      = Components.interfaces.nsIFilePicker.returnOK;
const PICK_CANCEL  = Components.interfaces.nsIFilePicker.returnCancel;
const PICK_REPLACE = Components.interfaces.nsIFilePicker.returnReplace;

const FILTER_ALL    = Components.interfaces.nsIFilePicker.filterAll;
const FILTER_HTML   = Components.interfaces.nsIFilePicker.filterHTML;
const FILTER_TEXT   = Components.interfaces.nsIFilePicker.filterText;
const FILTER_IMAGES = Components.interfaces.nsIFilePicker.filterImages;
const FILTER_XML    = Components.interfaces.nsIFilePicker.filterXML;
const FILTER_XUL    = Components.interfaces.nsIFilePicker.filterXUL;

// evald f = fopen("/home/rginda/foo.txt", MODE_WRONLY | MODE_CREATE)
// evald f = fopen("/home/rginda/vnk.txt", MODE_RDONLY)

var futils = new Object();

futils.umask = PERM_IWOTH | PERM_IWGRP;
futils.MSG_SAVE_AS = "Save As";
futils.MSG_OPEN = "Open";

futils.getPicker =
function futils_nosepicker(initialPath, typeList, attribs)
{
    const classes = Components.classes;
    const interfaces = Components.interfaces;

    const PICKER_CTRID = "@mozilla.org/filepicker;1";
    const LOCALFILE_CTRID = "@mozilla.org/file/local;1";

    const nsIFilePicker = interfaces.nsIFilePicker;
    const nsILocalFile = interfaces.nsILocalFile;

    var picker = classes[PICKER_CTRID].createInstance(nsIFilePicker);
    if (typeof attribs == "object")
    {
        for (var a in attribs)
            picker[a] = attribs[a];
    }
    else
        throw "bad type for param |attribs|";
    
    if (initialPath)
    {
        var localFile;
        
        if (typeof initialPath == "string")
        {
            localFile =
                classes[LOCALFILE_CTRID].createInstance(nsILocalFile);
            localFile.initWithPath(initialPath);
        }
        else
        {
            if (!(initialPath instanceof nsILocalFile))
                throw "bad type for argument |initialPath|";

            localFile = initialPath;
        }
        
        picker.displayDirectory = localFile
    }

    if (typeof typeList == "string")
        typeList = typeList.split(" ");

    if (typeList instanceof Array)
    {
        for (var i in typeList)
        {
            switch (typeList[i])
            {
                case "$all":
                    picker.appendFilters(FILTER_ALL);
                    break;
                    
                case "$html":
                    picker.appendFilters(FILTER_HTML);
                    break;
                    
                case "$text":
                    picker.appendFilters(FILTER_TEXT);
                    break;
                    
                case "$images":
                    picker.appendFilters(FILTER_IMAGES);
                    break;
                    
                case "$xml":
                    picker.appendFilters(FILTER_XML);
                    break;
                    
                case "$xul":
                    picker.appendFilters(FILTER_XUL);
                    break;

                default:
                    picker.appendFilter(typeList[i], typeList[i]);
                    break;
            }
        }
    }
 
    return picker;
}

function pickSaveAs (title, typeList, defaultFile, defaultDir)
{
    if (!defaultDir && "lastSaveAsDir" in futils)
        defaultDir = futils.lastSaveAsDir;
    
    var picker = futils.getPicker (defaultDir, typeList, 
                                   {defaultString: defaultFile});
    picker.init (window, title ? title : futils.MSG_SAVE_AS,
                 Components.interfaces.nsIFilePicker.modeSave);

    var rv = picker.show();
    
    if (rv != PICK_CANCEL)
        futils.lastSaveAsDir = picker.file.parent;

    return {reason: rv, file: picker.file, picker: picker};
}

function pickOpen (title, typeList, defaultFile, defaultDir)
{
    if (!defaultDir && "lastOpenDir" in futils)
        defaultDir = futils.lastOpenDir;
    
    var picker = futils.getPicker (defaultDir, typeList, 
                                   {defaultString: defaultFile});
    picker.init (window, title ? title : futils.MSG_OPEN,
                 Components.interfaces.nsIFilePicker.modeOpen);

    var rv = picker.show();
    
    if (rv != PICK_CANCEL)
        futils.lastOpenDir = picker.file.parent;

    return {reason: rv, file: picker.file, picker: picker};
}

function fopen (path, mode, perms, tmp)
{
    return new LocalFile(path, mode, perms, tmp);
}

function LocalFile(file, mode, perms, tmp)
{
    const classes = Components.classes;
    const interfaces = Components.interfaces;

    const LOCALFILE_CTRID = "@mozilla.org/file/local;1";
    const FILEIN_CTRID = "@mozilla.org/network/file-input-stream;1";
    const FILEOUT_CTRID = "@mozilla.org/network/file-output-stream;1";
    const SCRIPTSTREAM_CTRID = "@mozilla.org/scriptableinputstream;1";
    
    const nsIFile = interfaces.nsIFile;
    const nsILocalFile = interfaces.nsILocalFile;
    const nsIFileOutputStream = interfaces.nsIFileOutputStream;
    const nsIFileInputStream = interfaces.nsIFileInputStream;
    const nsIScriptableInputStream = interfaces.nsIScriptableInputStream;

    if (typeof perms == "undefined")
        perms = 0666 & ~futils.umask;
    
    if (typeof file == "string")
    {
        this.localFile = classes[LOCALFILE_CTRID].createInstance(nsILocalFile);
        this.localFile.initWithPath(file);
    }
    else if (file instanceof nsILocalFile)
    {
        this.localFile = file;
    }
    else if (file instanceof Array && file.length > 0)
    {
        this.localFile = classes[LOCALFILE_CTRID].createInstance(nsILocalFile);
        this.localFile.initWithPath(file.shift());
        while (file.length > 0)
            this.localFile.appendRelativePath(file.shift());
    }
    else
    {
        throw "bad type for argument |file|.";
    }
    
    if (mode & (MODE_WRONLY | MODE_RDWR))
    {
        this.outputStream = 
            classes[FILEOUT_CTRID].createInstance(nsIFileOutputStream);
        this.outputStream.init(this.localFile, mode, perms, 0);
    }
    
    if (mode & (MODE_RDONLY | MODE_RDWR))
    {
        var is = classes[FILEIN_CTRID].createInstance(nsIFileInputStream);
        is.init(this.localFile, mode, perms, tmp);
        this.inputStream =
            classes[SCRIPTSTREAM_CTRID].createInstance(nsIScriptableInputStream);
        this.inputStream.init(is);
    }    
}


LocalFile.prototype.write =
function fo_write(buf)
{
    if (!("outputStream" in this))
        throw "file not open for writing.";
    return this.outputStream.write(buf, buf.length);
}

LocalFile.prototype.read =
function fo_read(max)
{
    if (!("inputStream" in this))
        throw "file not open for reading.";

    var av = this.inputStream.available();
    if (typeof max == "undefined")
        max = av;

    if (!av)
        return null;    
    
    var rv = this.inputStream.read(max);
    return rv;
}

LocalFile.prototype.close =
function fo_close()
{
    if ("outputStream" in this)
        this.outputStream.close();
    if ("inputStream" in this)
        this.inputStream.close();
}

LocalFile.prototype.flush =
function fo_close()
{
    return this.outputStream.flush();
}


// -------------------------------------------------
// From utils.js
// XXX Rename this to global.js

// Whether or not to dump debugging messages to the console.
const DEBUG = true;
var debug;
if (DEBUG)
  debug = function(msg) { dump(' -- FZ -- : ' + msg + '\n'); }
else
  debug = function() {}

var rdf =
  Components
    .classes["@mozilla.org/rdf/rdf-service;1"]
      .getService(Components.interfaces.nsIRDFService);

const RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const RDF_TYPE = rdf.GetResource(RDF_NS + "type");

const RSS_NS = "http://purl.org/rss/1.0/";
const RSS_CHANNEL = rdf.GetResource(RSS_NS + "channel");
const RSS_TITLE = rdf.GetResource(RSS_NS + "title");
const RSS_DESCRIPTION = rdf.GetResource(RSS_NS + "description");
const RSS_ITEMS = rdf.GetResource(RSS_NS + "items");
const RSS_ITEM = rdf.GetResource(RSS_NS + "item");
const RSS_LINK = rdf.GetResource(RSS_NS + "link");

const RSS_CONTENT_NS = "http://purl.org/rss/1.0/modules/content/";
const RSS_CONTENT_ENCODED = rdf.GetResource(RSS_CONTENT_NS + "encoded");

const DC_NS = "http://purl.org/dc/elements/1.1/";
const DC_CREATOR = rdf.GetResource(DC_NS + "creator");
const DC_SUBJECT = rdf.GetResource(DC_NS + "subject");
const DC_DATE = rdf.GetResource(DC_NS + "date");
const DC_TITLE = rdf.GetResource(DC_NS + "title");
const DC_IDENTIFIER = rdf.GetResource(DC_NS + "identifier");

const FZ_NS = "urn:forumzilla:";
const FZ_ROOT = rdf.GetResource(FZ_NS + "root");
const FZ_FEEDS = rdf.GetResource(FZ_NS + "feeds");
const FZ_FEED = rdf.GetResource(FZ_NS + "feed");
const FZ_QUICKMODE = rdf.GetResource(FZ_NS + "quickMode");
const FZ_DESTFOLDER = rdf.GetResource(FZ_NS + "destFolder");

// XXX There's a containerutils in forumzilla.js that this should be merged with.
var containerUtils =
  Components
    .classes["@mozilla.org/rdf/container-utils;1"]
      .getService(Components.interfaces.nsIRDFContainerUtils);

var fileHandler =
  Components
    .classes["@mozilla.org/network/io-service;1"]
      .getService(Components.interfaces.nsIIOService)
        .getProtocolHandler("file")
          .QueryInterface(Components.interfaces.nsIFileProtocolHandler);

function getNodeValue(node) {
  if (node && node.textContent)
    return node.textContent;
  else if (node && node.firstChild) {
    var ret = "";
    for (var child = node.firstChild; child; child = child.nextSibling) {
      var value = getNodeValue(child);
      if (value)
        ret += value;
    }

    if (ret)
      return ret;
  }

  return null;
}

function getRDFTargetValue(ds, source, property) {
  var node = ds.GetTarget(source, property, true);
  if (node) {
    node = node.QueryInterface(Components.interfaces.nsIRDFLiteral);
    if (node)
      return node.Value;
  }
  return null;
}

var gFzSubscriptionsDS; // cache
function getSubscriptionsDS() {
    if (gFzSubscriptionsDS)
        return gFzSubscriptionsDS;

    var file = getSubscriptionsFile();
    var url = fileHandler.getURLSpecFromFile(file);

    gFzSubscriptionsDS = rdf.GetDataSource(url);
    if (!gFzSubscriptionsDS)
        throw("can't get subscriptions data source");

    // Note that it this point the datasource may not be loaded yet.
    // You have to QueryInterface it to nsIRDFRemoteDataSource and check
    // its "loaded" property to be sure.  You can also attach an observer
    // which will get notified when the load is complete.

    return gFzSubscriptionsDS;
}

function getSubscriptionsList() {
    var ds = getSubscriptionsDS();
    var list = ds.GetTarget(FZ_ROOT, FZ_FEEDS, true);
    //list = feeds.QueryInterface(Components.interfaces.nsIRDFContainer);
    list = list.QueryInterface(Components.interfaces.nsIRDFResource);
    list = containerUtils.MakeSeq(ds, list);
    return list;
}

function getSubscriptionsFile() {
  // Get the app directory service so we can look up the user's profile dir.
  var appDirectoryService =
    Components
      .classes["@mozilla.org/file/directory_service;1"]
        .getService(Components.interfaces.nsIProperties);
  if ( !appDirectoryService )
    throw("couldn't get the directory service");

  // Get the user's profile directory.
  var profileDir =
    appDirectoryService.get("ProfD", Components.interfaces.nsIFile);
  if ( !profileDir )
    throw ("couldn't get the user's profile directory");

  // Get the user's subscriptions file.
  var file = profileDir.clone();
  file.append("feeds.rdf");

  // If the file doesn't exist, create it.
  if (!file.exists())
    createSubscriptionsFile(file);

  return file;
}

function createSubscriptionsFile(file) {
  file = new LocalFile(file, MODE_WRONLY | MODE_CREATE);
  file.write('\
<?xml version="1.0"?>\n\
<RDF:RDF xmlns:dc="http://purl.org/dc/elements/1.1/"\n\
         xmlns:fz="' + FZ_NS + '"\n\
         xmlns:RDF="http://www.w3.org/1999/02/22-rdf-syntax-ns#">\n\
  <RDF:Description about="' + FZ_ROOT.Value + '">\n\
    <fz:feeds>\n\
      <RDF:Seq>\n\
      </RDF:Seq>\n\
    </fz:feeds>\n\
  </RDF:Description>\n\
</RDF:RDF>\n\
');
  file.close();
}
