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
# The Original Code is the RSS Parsing Engine
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


// error codes used to inform the consumer about attempts to download a feed
const kNewsBlogSuccess = 0;
const kNewsBlogInvalidFeed = 1; // usually means there was an error trying to parse the feed...
const kNewsBlogRequestFailure = 2; // generic networking failure when trying to download the feed.
const kNewsBlogFeedIsBusy = 3;

// Cache for all of the feeds currently being downloaded, indexed by URL, so the load event listener
// can access the Feed objects after it finishes downloading the feed.
var FeedCache = 
{
  mFeeds: new Array(),

  putFeed: function (aFeed)
  {
    this.mFeeds[this.normalizeHost(aFeed.url)] = aFeed;
  },

  getFeed: function (aUrl)
  {
    return this.mFeeds[this.normalizeHost(aUrl)];
  },

  removeFeed: function (aUrl)
  {
    delete this.mFeeds[this.normalizeHost(aUrl)];
  },

  normalizeHost: function (aUrl)
  {
    normalizedUrl = Components.classes["@mozilla.org/network/standard-url;1"].
                    createInstance(Components.interfaces.nsIURI);
    normalizedUrl.spec = aUrl;    
    normalizedUrl.host = normalizedUrl.host.toLowerCase();
    return normalizedUrl.spec;
  }
};

function Feed(resource) 
{
  this.resource = resource.QueryInterface(Components.interfaces.nsIRDFResource);
  this.description = null;
  this.author = null;
 
  this.request = null;
  this.folder = null;
  this.server = null;
  this.downloadCallback = null;
  this.items = new Array();

  return this;
}

// The name of the message folder corresponding to the feed.
// XXX This should be called something more descriptive like "folderName".
// XXX Or maybe, when we support nested folders and downloading into any folder,
// there could just be a reference to the folder itself called "folder".
Feed.prototype.name getter = function() 
{
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

Feed.prototype.download = function(parseItems, aCallback) 
{
  this.downloadCallback = aCallback; // may be null 

  // Whether or not to parse items when downloading and parsing the feed.
  // Defaults to true, but setting to false is useful for obtaining
  // just the title of the feed when the user subscribes to it.
  this.parseItems = parseItems == null ? true : parseItems ? true : false;

  // Before we do anything...make sure the url is an http url. This is just a sanity check
  // so we don't try opening mailto urls, imap urls, etc. that the user may have tried to subscribe to 
  // as an rss feed..
  var uri = Components.classes["@mozilla.org/network/standard-url;1"].
                      createInstance(Components.interfaces.nsIURI);
  uri.spec = this.url;
  if (!(uri.schemeIs("http") || uri.schemeIs("https")))
    return this.onParseError(this); // simulate an invalid feed error

  // Before we try to download the feed, make sure we aren't already processing the feed
  // by looking up the url in our feed cache
  if (FeedCache.getFeed(this.url))
  {
    if (this.downloadCallback)
      this.downloadCallback.downloaded(this, kNewsBlogFeedIsBusy);
    return ; // don't do anything, the feed is already in use
  }

  this.request = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                 .createInstance(Components.interfaces.nsIXMLHttpRequest);
  this.request.onprogress = Feed.onProgress; // must be set before calling .open
  this.request.open("GET", this.url, true);

  this.request.overrideMimeType("text/xml");
  this.request.onload = Feed.onDownloaded;
  this.request.onerror = Feed.onDownloadError;
  FeedCache.putFeed(this);
  this.request.send(null);
}

Feed.onDownloaded = function(event) 
{
  var request = event.target;
  var url = request.channel.originalURI.spec;
  debug(url + " downloaded");
  var feed = FeedCache.getFeed(url);
  if (!feed)
    throw("error after downloading " + url + ": couldn't retrieve feed from request");
  
  feed.parse();

  // parse will asynchronously call the download callback when it is done
}

Feed.onProgress = function(event) 
{
  var request = event.target;
  var url = request.channel.originalURI.spec;
  var feed = FeedCache.getFeed(url);

  if (feed.downloadCallback)
    feed.downloadCallback.onProgress(feed, event.position, event.totalSize);
}

Feed.onDownloadError = function(event) 
{
  var request = event.target;
  var url = request.channel.originalURI.spec;
  var feed = FeedCache.getFeed(url);
  if (feed.downloadCallback)
    feed.downloadCallback.downloaded(feed, kNewsBlogRequestFailure);

  FeedCache.removeFeed(url);
}

Feed.prototype.onParseError = function(feed) 
{
  if (feed && feed.downloadCallback)
  {
    if (feed.downloadCallback)
      feed.downloadCallback.downloaded(feed, kNewsBlogInvalidFeed);
    FeedCache.removeFeed(url);
  }
}

Feed.prototype.url getter = function() 
{
  var ds = getSubscriptionsDS(this.server);
  var url = ds.GetTarget(this.resource, DC_IDENTIFIER, true);
  if (url)
    url = url.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  else
    url = this.resource.Value;
  return url;
}

Feed.prototype.title getter = function() 
{
  var ds = getSubscriptionsDS(this.server);
  var title = ds.GetTarget(this.resource, DC_TITLE, true);
  if (title)
    title = title.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  return title;
}

Feed.prototype.title setter = function(new_title) 
{
  var ds = getSubscriptionsDS(this.server);
  new_title = rdf.GetLiteral(new_title || "");
  var old_title = ds.GetTarget(this.resource, DC_TITLE, true);
  if (old_title)
      ds.Change(this.resource, DC_TITLE, old_title, new_title);
  else
      ds.Assert(this.resource, DC_TITLE, new_title, true);
}

Feed.prototype.quickMode getter = function() 
{
  var ds = getSubscriptionsDS(this.server);
  var quickMode = ds.GetTarget(this.resource, FZ_QUICKMODE, true);
  if (quickMode) 
  {
    quickMode = quickMode.QueryInterface(Components.interfaces.nsIRDFLiteral);
    quickMode = quickMode.Value;
    quickMode = eval(quickMode);
  }    
  return quickMode;
}

Feed.prototype.quickMode setter = function(new_quickMode) 
{
  var ds = getSubscriptionsDS(this.server);
  new_quickMode = rdf.GetLiteral(new_quickMode || "");
  var old_quickMode = ds.GetTarget(this.resource, FZ_QUICKMODE, true);
  if (old_quickMode)
    ds.Change(this.resource, FZ_QUICKMODE, old_quickMode, new_quickMode);
  else
    ds.Assert(this.resource, FZ_QUICKMODE, new_quickMode, true);
}

Feed.prototype.parse = function() 
{
  // Figures out what description language (RSS, Atom) and version this feed
  // is using and calls a language/version-specific feed parser.

  debug("parsing feed " + this.url);

  if (!this.request.responseText) 
    return this.onParseError(this);

  // create a feed parser which will parse the feed for us
  var parser = new FeedParser();
  this.itemsToStore = parser.parseFeed(this, this.request.responseText, this.request.responseXML, this.request.channel.URI);
  
  // storeNextItem will iterate through the parsed items, storing each one.
  this.itemsToStoreIndex = 0;
  this.storeNextItem();
}

Feed.prototype.invalidateItems = function () 
{
  var ds = getItemsDS(this.server);
  debug("invalidating items for " + this.url);
  var items = ds.GetSources(FZ_FEED, this.resource, true);
  var item;
  
  while (items.hasMoreElements()) 
  {
    item = items.getNext();
    item = item.QueryInterface(Components.interfaces.nsIRDFResource);
    debug("invalidating " + item.Value);
    var valid = ds.GetTarget(item, FZ_VALID, true);
    if (valid)
      ds.Unassert(item, FZ_VALID, valid, true);
  }
}

Feed.prototype.removeInvalidItems = function() 
{
  var ds = getItemsDS(this.server);
  debug("removing invalid items for " + this.url);
  var items = ds.GetSources(FZ_FEED, this.resource, true);
  var item;
  while (items.hasMoreElements()) 
  {
    item = items.getNext();
    item = item.QueryInterface(Components.interfaces.nsIRDFResource);
    if (ds.HasAssertion(item, FZ_VALID, RDF_LITERAL_TRUE, true))
      continue;
    debug("removing " + item.Value);
    ds.Unassert(item, FZ_FEED, this.resource, true);
    if (ds.hasArcOut(item, FZ_FEED))
      debug(item.Value + " is from more than one feed; only the reference to this feed removed");
    else
      removeAssertions(ds, item);
  }
}

// gets the next item from gItemsToStore and forces that item to be stored
// to the folder. If more items are left to be stored, fires a timer for the next one.
// otherwise it triggers a download done notification to the UI
Feed.prototype.storeNextItem = function()
{
  if (!this.itemsToStore.length)
  {
    // create a folder for the feed if one does not exist already...
    var folder;

    try {
        folder = this.server.rootMsgFolder.getChildNamed(this.name);
     } catch(e) {}

    if (!folder) 
      this.server.rootMsgFolder.createSubfolder(this.name, null /* supposed to be a msg window */);

    return this.cleanupParsingState(this);
  }

  var item = this.itemsToStore[this.itemsToStoreIndex]; 

  item.store();
  item.markValid();

  // if the listener is tracking progress for storing each item, report it here...
  if (item.feed.downloadCallback && item.feed.downloadCallback.onFeedItemStored)
    item.feed.downloadCallback.onFeedItemStored(item.feed, this.itemsToStoreIndex, this.itemsToStore.length);
 
  this.itemsToStoreIndex++

  // eventually we'll report individual progress here....

  if (this.itemsToStoreIndex < this.itemsToStore.length)
  {
    if (!this.storeItemsTimer)
      this.storeItemsTimer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
    this.storeItemsTimer.initWithCallback(this, 50, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
  }
  else
    this.cleanupParsingState(item.feed);   
}

Feed.prototype.cleanupParsingState = function(feed) {
    // now that we are done parsing the feed, remove the feed from our feed cache
  FeedCache.removeFeed(feed.url);

  feed.removeInvalidItems();

  // let's be sure to flush any feed item changes back to disk
  var ds = getItemsDS(feed.server);
  ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush(); // flush any changes

  if (feed.downloadCallback)
    feed.downloadCallback.downloaded(feed, kNewsBlogSuccess);

  this.request = null; // force the xml http request to go away. This helps reduce some nasty assertions on shut down. 
  this.itemsToStore = "";
  this.itemsToStoreIndex = 0;
  this.storeItemsTimer = null;
}   

Feed.prototype.notify = function(aTimer) 
{
  this.storeNextItem();
}

Feed.prototype.QueryInterface = function(aIID) 
{
  if (aIID.equals(Components.interfaces.nsITimerCallback) || aIID.equals(Components.interfaces.nsISupports))
    return this;

  Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
  return null;    
}
