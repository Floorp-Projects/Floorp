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
 * The Original Code is the RSS Subscription UI
 *
 *
 * Contributor(s):
 *  Myk Melez <myk@mozilla.org) (Original Author)
 *  David Bienvenu <bienvenu@nventure.com> 
 *  Scott MacGregor <mscott@mozilla.org>
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

var kFeedUrlDelimiter = '|'; // the delimiter used to delimit feed urls in the msg folder database "feedUrl" property
var gRSSServer = null;

function doLoad() {
    // extract the server argument
    if (window.arguments[0].server)
        gRSSServer = window.arguments[0].server;
    
    var ds = getSubscriptionsDS(gRSSServer);
    var tree = document.getElementById('subscriptions');
    tree.database.AddDataSource(ds);
    tree.builder.rebuild();
}

// opens the feed properties dialog
// optionally, pass in the name and 
function openFeedEditor(feedProperties)
{
    window.openDialog('feed-properties.xul', 'feedproperties', 'modal,titlebar,chrome,center', feedProperties);
    return feedProperties;
} 

// status helper routines

function updateStatusItem(aID, aValue)
{
  var el = document.getElementById(aID);
  if (el.getAttribute('collapsed'))
    el.removeAttribute('collapsed');

  el.value = aValue;
}

function clearStatusInfo()
{
  document.getElementById('statusText').value = "";
  document.getElementById('progressMeter').collapsed = true;
}

var feedDownloadCallback = {
  downloaded: function(feed, aSuccess)
  {
    // feed is null if our attempt to parse the feed failed
    if (aSuccess)
    {
      updateStatusItem('progressMeter', 100);

      // if we get here...we should always have a folder by now...either
      // in feed.folder or FeedItems created the folder for us....
      var folder = feed.folder ? feed.folder : gRSSServer.rootMsgFolder.getChildNamed(feed.name);

      updateFolderFeedUrl(folder, feed.url, false);

      // add feed just adds the feed we have validated and downloaded to the subscription UI. 
      // it also flushes the subscription datasource
      addFeed(feed.url, feed.name, null, folder); 
    } 
    else 
    {
      // Add some code to alert the user that the feed was not something we could understand...  
    }

    // our operation is done...clear out the status text and progressmeter
    setTimeout(clearStatusInfo, 1000);
  },

  // this gets called after the RSS parser finishes storing a feed item to disk
  // aCurrentFeedItems is an integer corresponding to how many feed items have been downloaded so far
  // aMaxFeedItems is an integer corresponding to the total number of feed items to download
  onFeedItemStored: function (feed, aCurrentFeedItems, aMaxFeedItems)
  { 
    updateStatusItem('statusText', 
      document.getElementById("bundle_newsblog").getFormattedString("subscribe-fetchingFeedItems", [aCurrentFeedItems, aMaxFeedItems]) );
    this.onProgress(feed, aCurrentFeedItems, aMaxFeedItems);
  },

  onProgress: function(feed, aProgress, aProgressMax)
  {
    updateStatusItem('progressMeter', (aProgress * 100) / aProgressMax);
  },
}

// updates the "feedUrl" property in the message database for the folder in question.
function updateFolderFeedUrl(aFolder, aFeedUrl, aRemoveUrl)
{
  var msgdb = aFolder.QueryInterface(Components.interfaces.nsIMsgFolder).getMsgDatabase(null);
  var folderInfo = msgdb.dBFolderInfo;
  var oldFeedUrl = folderInfo.getCharPtrProperty("feedUrl");

  if (aRemoveUrl)
  { 
    // remove our feed url string from the list of feed urls
    var newFeedUrl = oldFeedUrl.replace(kFeedUrlDelimiter + aFeedUrl, "");
    folderInfo.setCharPtrProperty("feedUrl", newFeedUrl);
  }  
  else 
    folderInfo.setCharPtrProperty("feedUrl", oldFeedUrl + kFeedUrlDelimiter + aFeedUrl);  
}

function doAdd() {
    var userAddedFeed = false; 
    var feedProperties = { feedName: "", feedLocation: "", serverURI: gRSSServer.serverURI, folderURI: "", result: userAddedFeed};

    feedProperties = openFeedEditor(feedProperties);

    // if the user hit cancel, exit without doing anything
    if (!feedProperties.result)
      return;
    
    if (!feedProperties.feedLocation)
        return;

    var itemResource = rdf.GetResource(feedProperties.feedLocation);
    feed = new Feed(itemResource);

    // if the user specified a specific folder to add the feed too, then set it here
    if (feedProperties.folderURI)
    {
      var folderResource = rdf.GetResource(feedProperties.folderURI);   
      if (folderResource)
        feed.folder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    }

    // set the server for the feed
    feed.server = gRSSServer;

    // update status text
    updateStatusItem('statusText', document.getElementById("bundle_newsblog").getString('subscribe-validating'));
    updateStatusItem('progressMeter', 0);

    // validate the feed and download the articles
    // we used to pass false which caused us to skip parsing then we'd 
    // turn around and download the feed again so we could actually parse the items...
    // But now that this operation is asynch, just kick it off once...if we change this back
    // modify feedDownloadCallback.downloaded to parse the feed...
    feed.download(true, feedDownloadCallback);
}

function doEdit() {
    // XXX There should be some way of correlating feed RDF resources
    // with their corresponding Feed objects.  Perhaps in the end much
    // of this code could hang off methods of the Feed object.
    var ds = getSubscriptionsDS(gRSSServer);
    var tree = document.getElementById('subscriptions');
    var item = tree.view.getItemAtIndex(tree.view.selection.currentIndex);
    var resource = rdf.GetResource(item.id);
    var old_url = ds.GetTarget(resource, DC_IDENTIFIER, true);
    old_url = old_url ? old_url.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : "";

    var currentFolder = ds.GetTarget(resource, FZ_DESTFOLDER, true);
    var currentFolderURI = currentFolder.QueryInterface(Components.interfaces.nsIRDFResource).Value;

    currentFolder = rdf.GetResource(currentFolderURI).QueryInterface(Components.interfaces.nsIMsgFolder);
   
    var userModifiedFeed = false; 
    var feedProperties = { feedLocation: old_url, serverURI: gRSSServer.serverURI, folderURI: currentFolderURI, result: userModifiedFeed};

    feedProperties = openFeedEditor(feedProperties);
    if (!feedProperties.result) // did the user cancel?
        return;

    // did the user change the folder URI for storing the feed?
    if (feedProperties.folderURI && feedProperties.folderURI != currentFolderURI)
    {
      // unassert the older URI, add an assertion for the new URI...
      ds.Change(resource, FZ_DESTFOLDER, currentFolder, rdf.GetResource(feedProperties.folderURI));

      // we need to update the feed url attributes on the databases for each folder
      var folderResource = rdf.GetResource(feedProperties.folderURI);   
      var newFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
      currentFolder = rdf.GetResource(currentFolderURI).QueryInterface(Components.interfaces.nsIMsgFolder);

      updateFolderFeedUrl(currentFolder, old_url, true); // remove our feed url property from the current folder
      updateFolderFeedUrl(newFolder, feedProperties.feedLocation, false); // add our feed url property to the new folder

      currentFolder = newFolder; // the folder has changed
    }

    // check to see if the location changed
    if (feedProperties.feedLocation && feedProperties.feedLocation != old_url)
    {
      ds.Change(resource, DC_IDENTIFIER, rdf.GetLiteral(old_url), rdf.GetLiteral(feedProperties.feedLocation));
      // now update our feed url property on the destination folder
      updateFolderFeedUrl(currentFolder, old_url, false); // remove the old url
      updateFolderFeedUrl(currentFolder, feedProperties.feedLocation, true);  // add the new one
    }

    // feed = new Feed(item.id);
    // feed.download();
}

function doRemove() {
    var tree = document.getElementById('subscriptions');
    var item = tree.view.getItemAtIndex(tree.view.selection.currentIndex);
    var resource = rdf.GetResource(item.id);
    var feed = new Feed(resource);
    var ds = getSubscriptionsDS(gRSSServer);
    
    // remove the feed from the subscriptions ds
    var feeds = getSubscriptionsList(gRSSServer);
    var index = feeds.IndexOf(resource);
    if (index != -1)
        feeds.RemoveElementAt(index, false);

    // remove the feed property string from the folder data base
    var currentFolder = ds.GetTarget(resource, FZ_DESTFOLDER, true);
    var currentFolderURI = currentFolder.QueryInterface(Components.interfaces.nsIRDFResource).Value;
    currentFolder = rdf.GetResource(currentFolderURI).QueryInterface(Components.interfaces.nsIMsgFolder);
    
    var feedUrl = ds.GetTarget(resource, DC_IDENTIFIER, true);    
    ds.Unassert(resource, DC_IDENTIFIER, feedUrl, true);

    feedUrl = feedUrl ? feedUrl.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : "";

    updateFolderFeedUrl(currentFolder, feedUrl, true); // remove the old url

    // Remove all assertions about the feed from the subscriptions database.
    removeAssertions(ds, resource);
    ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush(); // flush any changes

    // Remove all assertions about items in the feed from the items database.
    var itemds = getItemsDS(gRSSServer);
    feed.invalidateItems();
    feed.removeInvalidItems();
    itemds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush(); // flush any changes

    // If we don't have any more subscriptions pointing into
    // this folder, then I think we should offer to delete it...
    // Cheat and look at the feed url property to see if anyone else is still using the feed...
    // you could also accomplish this by looking at some properties in the data source...

    var msgdb = currentFolder.QueryInterface(Components.interfaces.nsIMsgFolder).getMsgDatabase(null);
    var folderInfo = msgdb.dBFolderInfo;
    var oldFeedUrl = folderInfo.getCharPtrProperty("feedUrl");

    if (!oldFeedUrl) // no more feeds pointing to the folder?
    {
      try {
        var openerResource = gRSSServer.rootMsgFolder.QueryInterface(Components.interfaces.nsIRDFResource);
        var folderResource = currentFolder.QueryInterface(Components.interfaces.nsIRDFResource);
        window.opener.messenger.DeleteFolders(window.opener.GetFolderDatasource(), openerResource, folderResource);
      } catch (e) { }
    }
}

/*
 * Disabled/Future Stuff
 */

/*
var subscriptionsDragDropObserver = {
    onDragStart: function (aEvent, aXferData, aDragAction) {
        if (aEvent.originalTarget.localName != 'treechildren')
            return;

        var tree = document.getElementById('subscriptions');
        var row = tree.treeBoxObject.selection.currentIndex;
        window.status = "drag started at " + row;

        aXferData.data = new TransferData();
        aXferData.data.addDataForFlavour("text/unicode", row);
    },
    getSupportedFlavours : function () {
        var flavours = new FlavourSet();
        flavours.appendFlavour("text/unicode");
        return flavours;
    },
    onDragOver: function (evt, flavour, session) {
        var now = new Date();
        //window.status = "dragging over " + now;
    },
    onDrop: function (evt, dropdata, session) {
        var url = dropdata.data.slice(0, dropdata.data.indexOf(" "));
        var name = dropdata.data.slice(dropdata.data.indexOf(" ")+1);
        window.status = "dropping " + name + "; " + url;

        subscribeToBlog(url, name);

    }
};

var directoryDragObserver = {
    onDragStart: function (aEvent, aXferData, aDragAction) {
        if (aEvent.originalTarget.localName != 'treechildren')
            return;

        var tree = document.getElementById("directory");
        var name = tree.view.getCellText(tree.currentIndex, "feeds-name-column");
        var url = tree.view.getCellValue(tree.currentIndex, "feeds-name-column");
        window.status = "dragging " + url + " " + name;

        aXferData.data = new TransferData();
        aXferData.data.addDataForFlavour("text/unicode", url + " " + name);
    }
};

//var gFoldersBeingDeleted = [];
//var gFolderDeletionsCompleted = 0;
// Supposedly watches an asynchronous message copy, but doesn't seem
// to get called. I suspect the copy is actually synchronous, which is better
// in any case, since this listener doesn't actually know which folder
// it gets notifications about, so it has to rely on a very clunky mechanism
// for figuring out when it can delete the folders.
//var MessageCopyListener = {
//    OnStartCopy: function() { alert('copy started'); },
//    OnProgress: function() {},
//    SetMessageKey: function() {},
//    GetMessageId: function() {},
//    OnStopCopy: function(aStatus, bar, baz) {
//        alert(aStatus + " " + bar + " " + baz);
//        ++gFolderDeletionsCompleted;
//        if (gFoldersBeingDeleted.length == gFolderDeletionsCompleted) {
//            for (i in gFoldersBeingDeleted)
//                gFoldersBeingDeleted[i].Delete();
//            gFoldersBeingDeleted = [];
//            gFolderDeletionsCompleted = 0;
//        }
//    },
//}

*/
