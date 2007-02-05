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
const kNewsBlogNoNewItems = 4; // there are no new articles for this feed

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
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].
                    getService(Components.interfaces.nsIIOService);
    var normalizedUrl = ioService.newURI(aUrl, null, null);
    normalizedUrl.host = normalizedUrl.host.toLowerCase();
    return normalizedUrl.spec;
  }
};

function Feed(aResource, aRSSServer) 
{
  this.resource = aResource.QueryInterface(Components.interfaces.nsIRDFResource);
  this.server = aRSSServer;
}

Feed.prototype = 
{
  description: null,
  author: null,
  request: null,
  server: null,
  downloadCallback: null,
  resource: null,
  items: new Array(),
  mFolder: null,

  get folder()
  {
    if (!this.mFolder)
    {
      try 
      {
        this.mFolder = this.server.rootMsgFolder.getChildNamed(this.name);
      } catch (ex) {}
    }

    return this.mFolder;
  },

  set folder (aFolder) 
  {
    this.mFolder = aFolder;
  },

  get name()
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
  },

  download: function(aParseItems, aCallback) 
  { 
    this.downloadCallback = aCallback; // may be null 

    // Whether or not to parse items when downloading and parsing the feed.
    // Defaults to true, but setting to false is useful for obtaining
    // just the title of the feed when the user subscribes to it.
    this.parseItems = aParseItems == null ? true : aParseItems ? true : false;

    // Before we do anything...make sure the url is an http url. This is just a sanity check
    // so we don't try opening mailto urls, imap urls, etc. that the user may have tried to subscribe to 
    // as an rss feed..
    var uri = Components.classes["@mozilla.org/network/standard-url;1"].
                        createInstance(Components.interfaces.nsIURI);
    uri.spec = this.url;
    if (!(uri.schemeIs("http") || uri.schemeIs("https")))
    {
      this.onParseError(this); // simulate an invalid feed error
      return;
    }

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
    this.request.onprogress = this.onProgress; // must be set before calling .open
    this.request.open("GET", this.url, true);

    var lastModified = this.lastModified;
    if (lastModified)
      this.request.setRequestHeader("If-Modified-Since", lastModified);

    this.request.overrideMimeType("text/xml");
    this.request.onload = this.onDownloaded;
    this.request.onerror = this.onDownloadError;
    FeedCache.putFeed(this);
    this.request.send(null);
  }, 

  onDownloaded: function(aEvent) 
  {
    var request = aEvent.target;
    var url = request.channel.originalURI.spec;
    debug(url + " downloaded");
    var feed = FeedCache.getFeed(url);
    if (!feed)
      throw("error after downloading " + url + ": couldn't retrieve feed from request");

    // if the request has a Last-Modified header on it, then go ahead and remember
    // that as a property on the feed so we can use it when making future requests.    
    var lastModifiedHeader = request.getResponseHeader('Last-Modified');
    if (lastModifiedHeader)
      this.lastModified = lastModifiedHeader;

    feed.parse(); // parse will asynchronously call the download callback when it is done
  }, 
  
  onProgress: function(aEvent) 
  {
    var request = aEvent.target;
    var url = request.channel.originalURI.spec;
    var feed = FeedCache.getFeed(url);

    if (feed.downloadCallback)
      feed.downloadCallback.onProgress(feed, aEvent.position, aEvent.totalSize);
  },

  onDownloadError: function(aEvent) 
  {
    var request = aEvent.target;
    var url = request.channel.originalURI.spec;
    var feed = FeedCache.getFeed(url);
    if (feed.downloadCallback) 
    {
      // if the http status code is a 304, then the feed has not been modified since we last downloaded it.
      var error = kNewsBlogRequestFailure;
      try
      {
        if (request.status == 304)
          error = kNewsBlogNoNewItems;
      } catch (ex) {}
      feed.downloadCallback.downloaded(feed, error);
    }
    
    FeedCache.removeFeed(url);
  },

  onParseError: function(aFeed) 
  {
    if (aFeed && aFeed.downloadCallback)
    {
      if (aFeed.downloadCallback)
        aFeed.downloadCallback.downloaded(aFeed, aFeed.request && aFeed.request.status == 304 ? kNewsBlogNoNewItems : kNewsBlogInvalidFeed);
      FeedCache.removeFeed(aFeed.url);
    }
  },

  get url()
  {
    var ds = getSubscriptionsDS(this.server);
    var url = ds.GetTarget(this.resource, DC_IDENTIFIER, true);
    if (url)
      url = url.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    else
      url = this.resource.Value;
    return url;
  },

  get title()
  {
    var ds = getSubscriptionsDS(this.server);
    var title = ds.GetTarget(this.resource, DC_TITLE, true);
    if (title)
      title = title.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    return title;
  },

  set title (aNewTitle) 
  {
    var ds = getSubscriptionsDS(this.server);
    aNewTitle = rdf.GetLiteral(aNewTitle);
    var old_title = ds.GetTarget(this.resource, DC_TITLE, true);
    if (old_title)
        ds.Change(this.resource, DC_TITLE, old_title, aNewTitle);
    else
        ds.Assert(this.resource, DC_TITLE, aNewTitle, true);
  },

  get lastModified()
  {
    var ds = getSubscriptionsDS(this.server);
    var lastModified = ds.GetTarget(this.resource, DC_LASTMODIFIED, true);
    if (lastModified)
      lastModified = lastModified.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    return lastModified;
  },

  set lastModified(aLastModified)
  {
    var ds = getSubscriptionsDS(this.server);
    aLastModified = rdf.GetLiteral(aLastModified);
    var old_lastmodified = ds.GetTarget(this.resource, DC_LASTMODIFIED, true);
    if (old_lastmodified)
        ds.Change(this.resource, DC_LASTMODIFIED, old_lastmodified, aLastModified);
    else
        ds.Assert(this.resource, DC_LASTMODIFIED, aLastModified, true);  

    // do we need to flush every time this property changes? 
    ds = ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);    
    ds.Flush();  
  },

  get quickMode ()
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
  },

  set quickMode (aNewQuickMode) 
  {
    var ds = getSubscriptionsDS(this.server);
    aNewQuickMode = rdf.GetLiteral(aNewQuickMode);
    var old_quickMode = ds.GetTarget(this.resource, FZ_QUICKMODE, true);
    if (old_quickMode)
      ds.Change(this.resource, FZ_QUICKMODE, old_quickMode, aNewQuickMode);
    else
      ds.Assert(this.resource, FZ_QUICKMODE, aNewQuickMode, true);
  },

  get link ()
  {
    var ds = getSubscriptionsDS(this.server);
    var link = ds.GetTarget(this.resource, RSS_LINK, true);
    if(link)
      link = link.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    return link;
  },

  set link (aNewLink)
  {
    if (!aNewLink)
      return;

    var ds = getSubscriptionsDS(this.server);
    aNewLink = rdf.GetLiteral(aNewLink);
    var old_link = ds.GetTarget(this.resource, RSS_LINK, true);
    if (old_link)
      ds.Change(this.resource, RSS_LINK, old_link, aNewLink);
    else
      ds.Assert(this.resource, RSS_LINK, aNewLink, true);
  },

  parse: function() 
  {
    // Figures out what description language (RSS, Atom) and version this feed
    // is using and calls a language/version-specific feed parser.

    debug("parsing feed " + this.url);

    if (!this.request.responseText) 
    {
      this.onParseError(this);
      return;
    }
      
    // create a feed parser which will parse the feed for us
    var parser = new FeedParser();
    this.itemsToStore = parser.parseFeed(this, this.request.responseText, this.request.responseXML, this.request.channel.URI);
  
    // storeNextItem will iterate through the parsed items, storing each one.
    this.itemsToStoreIndex = 0;
    this.storeNextItem();
  },

  invalidateItems: function () 
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
  }, 

  removeInvalidItems: function() 
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
  },

  createFolder: function()
  {   
    if (!this.folder) 
      this.server.rootMsgFolder.createSubfolder(this.name, null /* supposed to be a msg window */);      
  },

  // gets the next item from gItemsToStore and forces that item to be stored
  // to the folder. If more items are left to be stored, fires a timer for the next one.
  // otherwise it triggers a download done notification to the UI
  storeNextItem: function()
  {
    if (!this.itemsToStore ||  !this.itemsToStore.length)
    {
      this.createFolder();
      this.cleanupParsingState(this);
      return;
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
    {
      // we have just finished downloading one or more feed items into the destination folder,
      // if the folder is still listed as having new messages in it, then we should set the biff state on the folder
      // so the right RDF UI changes happen in the folder pane to indicate new mail.

      if (item.feed.folder.hasNewMessages)
        item.feed.folder.biffState = Components.interfaces.nsIMsgFolder.nsMsgBiffState_NewMail;
      this.cleanupParsingState(item.feed);   
    }
  },

  cleanupParsingState: function(aFeed) 
  {
    // now that we are done parsing the feed, remove the feed from our feed cache
    FeedCache.removeFeed(aFeed.url);
    aFeed.removeInvalidItems();

    // let's be sure to flush any feed item changes back to disk
    var ds = getItemsDS(aFeed.server);
    ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush(); // flush any changes

    if (aFeed.downloadCallback)
      aFeed.downloadCallback.downloaded(aFeed, kNewsBlogSuccess);

    this.request = null; // force the xml http request to go away. This helps reduce some nasty assertions on shut down. 
    this.itemsToStore = "";
    this.itemsToStoreIndex = 0;
    this.storeItemsTimer = null;
  }, 

  notify: function(aTimer) 
  {
    this.storeNextItem();
  }, 

  QueryInterface: function(aIID) 
  {
    if (aIID.equals(Components.interfaces.nsITimerCallback) || aIID.equals(Components.interfaces.nsISupports))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;    
  }
};

