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

var gExternalScriptsLoaded = false;

var nsNewsBlogFeedDownloader =
{
  downloadFeed: function(aUrl, aFolder, aQuickMode, aTitle, aUrlListener, aMsgWindow)
  {
    if (!gExternalScriptsLoaded)
      loadScripts();

    // aUrl may be a delimited list of feeds for a particular folder. We need to kick off a download
    // for each feed.

    var feedUrlArray = aUrl.split("|");

    // we might just pull all these args out of the aFolder DB, instead of passing them in...
    var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
        .getService(Components.interfaces.nsIRDFService);

    progressNotifier.init(aMsgWindow.statusFeedback, false);

    for (url in feedUrlArray)
    {
      if (feedUrlArray[url])
      {        
        id = rdf.GetResource(feedUrlArray[url]);
        feed = new Feed(id);
        feed.folder = aFolder;
        feed.server = aFolder.server;
        gNumPendingFeedDownloads++; // bump our pending feed download count
        feed.download(true, progressNotifier);
      }
    }
  },

  subscribeToFeed: function(aUrl, aFolder, aMsgWindow)
  {
    if (!gExternalScriptsLoaded)
      loadScripts();
    var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
              .getService(Components.interfaces.nsIRDFService);
    
    var itemResource = rdf.GetResource(aUrl);
    var feed = new Feed(itemResource);
    feed.server = aFolder.server;
    if (!aFolder.isServer) // if the root server, create a new folder for the feed
      feed.folder = aFolder; // user must want us to add this subscription url to an existing RSS folder.

    progressNotifier.init(aMsgWindow.statusFeedback, true);
    gNumPendingFeedDownloads++;
    feed.download(true, progressNotifier);
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

var nsNewsBlogAcctMgrExtension = 
{ 
  name: "newsblog",
  chromePackageName: "messenger-newsblog",
  showPanel: function (server)
  {
    return server.type == "rss";
  },
  QueryInterface: function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIMsgAccountManagerExtension) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  }  
}

var nsNewsBlogFeedDownloaderModule =
{
  getClassObject: function(aCompMgr, aCID, aIID)
  {
    if (!aIID.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    for (var key in this.mObjects) 
      if (aCID.equals(this.mObjects[key].CID))
        return this.mObjects[key].factory;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  mObjects: 
  {
    feedDownloader: 
    { 
      CID: Components.ID("{5c124537-adca-4456-b2b5-641ab687d1f6}"),
      contractID: "@mozilla.org/newsblog-feed-downloader;1",
      className: "News+Blog Feed Downloader",
      factory: 
      {
        createInstance: function (aOuter, aIID) 
        {
          if (aOuter != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
          if (!aIID.equals(Components.interfaces.nsINewsBlogFeedDownloader) &&
              !aIID.equals(Components.interfaces.nsISupports))
            throw Components.results.NS_ERROR_INVALID_ARG;

          // return the singleton
          return nsNewsBlogFeedDownloader.QueryInterface(aIID);
        }       
      } // factory
    }, // feed downloader
    
    nsNewsBlogAcctMgrExtension: 
    { 
      CID: Components.ID("{E109C05F-D304-4ca5-8C44-6DE1BFAF1F74}"),
      contractID: "@mozilla.org/accountmanager/extension;1?name=newsblog",
      className: "News+Blog Account Manager Extension",
      factory: 
      {
        createInstance: function (aOuter, aIID) 
        {
          if (aOuter != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
          if (!aIID.equals(Components.interfaces.nsIMsgAccountManagerExtension) &&
              !aIID.equals(Components.interfaces.nsISupports))
            throw Components.results.NS_ERROR_INVALID_ARG;

          // return the singleton
          return nsNewsBlogAcctMgrExtension.QueryInterface(aIID);
        }       
      } // factory
    } // account manager extension
  },

  registerSelf: function(aCompMgr, aFileSpec, aLocation, aType)
  {        
    aCompMgr = aCompMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    for (var key in this.mObjects) 
    {
      var obj = this.mObjects[key];
      aCompMgr.registerFactoryLocation(obj.CID, obj.className, obj.contractID, aFileSpec, aLocation, aType);
    }

    // we also need to do special account extension registration
    var catman = Components.classes["@mozilla.org/categorymanager;1"].getService(Components.interfaces.nsICategoryManager);
    catman.addCategoryEntry("mailnews-accountmanager-extensions",
                            "newsblog account manager extension",
                            "@mozilla.org/accountmanager/extension;1?name=newsblog", true, true);
  },

  unregisterSelf: function(aCompMgr, aFileSpec, aLocation)
  {
    aCompMgr = aCompMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    for (var key in this.mObjects) 
    {
      var obj = this.mObjects[key];
      aCompMgr.unregisterFactoryLocation(obj.CID, aFileSpec);
    }

    // unregister the account manager extension
    catman = Components.classes["@mozilla.org/categorymanager;1"].getService(Components.interfaces.nsICategoryManager);
    catman.deleteCategoryEntry("mailnews-accountmanager-extensions",
                               "@mozilla.org/accountmanager/extension;1?name=newsblog", true);
  },

  canUnload: function(aCompMgr)
  {
    return true;
  }
};

function NSGetModule(aCompMgr, aFileSpec)
{
  return nsNewsBlogFeedDownloaderModule;
}

function loadScripts()
{
  var scriptLoader =  Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                     .createInstance(Components.interfaces.mozIJSSubScriptLoader);
  if (scriptLoader)
  { 
    scriptLoader.loadSubScript("chrome://messenger-newsblog/content/Feed.js");
    scriptLoader.loadSubScript("chrome://messenger-newsblog/content/FeedItem.js");
    scriptLoader.loadSubScript("chrome://messenger-newsblog/content/file-utils.js");
    scriptLoader.loadSubScript("chrome://messenger-newsblog/content/utils.js");
  }

  gExternalScriptsLoaded = true;
}

// Progress glue code. Acts as a go between the RSS back end and the mail window front end
// determined by the aMsgWindow parameter passed into nsINewsBlogFeedDownloader.
// gNumPendingFeedDownloads: keeps track of the total number of feeds we have been asked to download
//                           this number may not reflect the # of entries in our mFeeds array because not all
//                           feeds may have reported in for the first time...
var gNumPendingFeedDownloads = 0;

var progressNotifier = {
  mSubscribeMode: false, 
  mStatusFeedback: null,
  mFeeds: new Array,

  init: function(aStatusFeedback, aSubscribeMode)
  {
    if (!gNumPendingFeedDownloads) // if we aren't already in the middle of downloading feed items...
    {
      this.mStatusFeedback = aStatusFeedback;
      this.mSubscribeMode = aSubscribeMode;
      this.mStatusFeedback.startMeteors();

      this.mStatusFeedback.showStatusString(aSubscribeMode ? GetNewsBlogStringBundle().GetStringFromName('subscribe-validating') 
                                            : GetNewsBlogStringBundle().GetStringFromName('newsblog-getNewMailCheck'));
    }
  },

  downloaded: function(feed, aErrorCode)
  {
    if (this.mSubscribeMode && aErrorCode == kNewsBlogSuccess)
    {
      // if we get here...we should always have a folder by now...either
      // in feed.folder or FeedItems created the folder for us....
      var folder = feed.folder ? feed.folder : feed.server.rootMsgFolder.getChildNamed(feed.name);
      updateFolderFeedUrl(folder, feed.url, false);        
      addFeed(feed.url, feed.name, null, folder); // add feed just adds the feed to the subscription UI and flushes the datasource
    } 
    else if (aErrorCode == kNewsBlogInvalidFeed)
      this.mStatusFeedback.showStatusString(GetNewsBlogStringBundle().formatStringFromName("newsblog-invalidFeed",
                                            [feed.url], 1));
    else if (aErrorCode == kNewsBlogRequestFailure)
      this.mStatusFeedback.showStatusString(GetNewsBlogStringBundle().formatStringFromName("newsblog-networkError",
                                            [feed.url], 1));
      
    this.mStatusFeedback.stopMeteors();
    gNumPendingFeedDownloads--;

    if (!gNumPendingFeedDownloads)
    {
      this.mFeeds = new Array;

      this.mSubscribeMode = false;

      // should we do this on a timer so the text sticks around for a little while? 
      // It doesnt look like we do it on a timer for newsgroups so we'll follow that model.
      if (aErrorCode == kNewsBlogSuccess) // don't clear the status text if we just dumped an error to the status bar!
        this.mStatusFeedback.showStatusString("");
    }
  },

  // this gets called after the RSS parser finishes storing a feed item to disk
  // aCurrentFeedItems is an integer corresponding to how many feed items have been downloaded so far
  // aMaxFeedItems is an integer corresponding to the total number of feed items to download
  onFeedItemStored: function (feed, aCurrentFeedItems, aMaxFeedItems)
  { 
    // we currently don't do anything here. Eventually we may add
    // status text about the number of new feed articles received.

    if (this.mSubscribeMode) // if we are subscribing to a feed, show feed download progress
    {
      this.mStatusFeedback.showStatusString(GetNewsBlogStringBundle().formatStringFromName("subscribe-fetchingFeedItems", [aCurrentFeedItems, aMaxFeedItems], 2));
      this.onProgress(feed, aCurrentFeedItems, aMaxFeedItems);
    }
  },

  onProgress: function(feed, aProgress, aProgressMax)
  {
    if (feed.url in this.mFeeds) // have we already seen this feed?
      this.mFeeds[feed.url].currentProgress = aProgress;
    else
      this.mFeeds[feed.url] = {currentProgress: aProgress, maxProgress: aProgressMax};
    
    this.updateProgressBar();     
  },

  updateProgressBar: function()
  {
    var currentProgress = 0;
    var maxProgress = 0;
    for (index in this.mFeeds)
    {
      currentProgress += this.mFeeds[index].currentProgress;
      maxProgress += this.mFeeds[index].maxProgress;
    }

    // if we start seeing weird "jumping" behavior where the progress bar goes below a threshold then above it again,
    // then we can factor a fudge factor here based on the number of feeds that have not reported yet and the avg
    // progress we've already received for existing feeds. Fortunately the progressmeter is on a timer
    // and only updates every so often. For the most part all of our request have initial progress
    // before the UI actually picks up a progress value. 

    var progress = (currentProgress * 100) / maxProgress;
    this.mStatusFeedback.showProgress(progress);
  }
}

function GetNewsBlogStringBundle(name)
{
  var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].getService(); 
  strBundleService = strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);
  var strBundle = strBundleService.createBundle("chrome://messenger-newsblog/locale/newsblog.properties"); 
  return strBundle;
}
