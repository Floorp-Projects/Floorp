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

function doLoad() {
    // Display the list of feed subscriptions.
    var file = getSubscriptionsFile();
    var ds = getSubscriptionsDS();
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
  downloaded: function(feed)
  {
    // feed is null if our attempt to parse the feed failed
    if (feed)
    {
      debug("after download, feed name = " + feed.name + "\n");

      updateStatusItem('progressMeter', 100);
      updateStatusItem('statusText', document.getElementById("bundle_newsblog").getString('subscribe-validFeedFound'));
      updateStatusItem('statusText', 'Valid Feed Found!');

      var server = getIncomingServer();
      var folder;
      try {
          var folder = server.rootMsgFolder.getChildNamed(feed.name);
      }
      catch(e) {
          // If we're here, it's probably because the folder doesn't exist yet,
          // so create it.
          debug("folder for new feed " + feed.name + " doesn't exist; creating");
			    debug("creating " + feed.name + "as child of " + server.rootMsgFolder + "\n");
          server.rootMsgFolder.createSubfolder(feed.name, getMessageWindow());
          folder = server.rootMsgFolder.FindSubFolder(feed.name);
          var msgdb = folder.getMsgDatabase(null);
          var folderInfo = msgdb.dBFolderInfo;
          folderInfo.setCharPtrProperty("feedUrl", feed.url);
      }

      // add feed just adds the feed we have validated and downloaded to the subscription UI. 
      // it also flushes the subscription database
      addFeed(feed.url, feed.name, null, folder); 
    } 
    else 
    {
      // Add some code to alert the user that the feed was not something we could understand...  
    }

    // our operation is done...clear out the status text and progressmeter
    setTimeout(clearStatusInfo, 1000);
  },

  onProgress: function(aProgress, aProgressMax)
  {
      updateStatusItem('progressMeter', (aProgress * 100) / aProgressMax);
  },
}

function doAdd() {
    var userAddedFeed = false; 
    var feedProperties = { feedName: "", feedLocation: "", result: userAddedFeed};

    feedProperties = openFeedEditor(feedProperties);

    // if the user hit cancel, exit without doing anything
    if (!feedProperties.result)
    {
      debug("feedProperties.result empty\n");
      return;
    }
    
    if (!feedProperties.feedLocation)
    {
        debug("feedProperties.feedLocation empty\n");
        return;
    }

    var itemResource = rdf.GetResource(feedProperties.feedLocation);
    feed = new Feed(itemResource);

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
    var ds = getSubscriptionsDS();
    var tree = document.getElementById('subscriptions');
    var item = tree.view.getItemAtIndex(tree.view.selection.currentIndex);
    var resource = rdf.GetResource(item.id);
    var old_title = ds.GetTarget(resource, DC_TITLE, true);
    old_title =
        old_title ? old_title.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : "";
    var old_url = ds.GetTarget(resource, DC_IDENTIFIER, true);
    old_url =
        old_url ? old_url.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : "";

    var userModifiedFeed = false; 
    var feedProperties = { feedName: old_title, feedLocation: old_url, result: userModifiedFeed};

    feedProperties = openFeedEditor(feedProperties);
    if (!feedProperties.result) // did the user cancel?
        return;

    if (feedProperties.feedName != old_title) {
        var server = getIncomingServer();
        var msgWindow = getMessageWindow();

        // Throwing an error 0x80004005 seems to be getChildNamed()'s way
        // of saying that a folder doesn't exist, so we need to trap that
        // since it isn't fatal in our case.  XXX We should probably check
        // the error code in the catch statement to verify the problem.
        var old_folder;
        try {
            old_folder = server.rootMsgFolder.getChildNamed(old_title);
            old_folder = old_folder.QueryInterface(Components.interfaces.nsIMsgFolder);
        } catch(e) {}
        var new_folder;
        try {
            new_folder = server.rootMsgFolder.getChildNamed(feedProperties.feedName);
            new_folder = new_folder.QueryInterface(Components.interfaces.nsIMsgFolder);
        } catch(e) {}

        if (old_folder && new_folder) {
            // We could move messages from the old folder to the new folder
            // and then delete the old folder (code for doing so is below),
            // but how do we distinguish between messages in the old folder
            // from this feed vs. from another feed?  Until we can do that,
            // which probably requires storing the feed ID in the message headers,
            // we're better off just leaving the old folder as it is.
            //var messages = old_folder.getMessages(msgWindow);
            //var movees =
            //    Components
            //        .classes["@mozilla.org/supports-array;1"]
            //            .createInstance(Components.interfaces.nsISupportsArray);
            //var message;
            //while (messages.hasMoreElements()) {
            //    message = messages.getNext();
            //    movees.AppendElement(message);
            //}
            //gFoldersBeingDeleted.push(old_folder);
            //new_folder.copyMessages(old_folder,
            //                        movees,
            //                        true /*isMove*/,
            //                        msgWindow /* nsIMsgWindow */,
            //                        null /* listener */,
            //                        false /* isFolder */,
            //                        false /*allowUndo*/ );
        }
        else if (old_folder) {
            // We could rename the old folder to the new folder
            // (code for doing so is below), but what if other feeds
            // are using the old folder?  Until we write code to determine
            // whether they are or not (and perhaps even then), better to leave
            // the old folder as it is and merely create a new folder.
            //old_folder.rename(new_title, msgWindow);
            server.rootMsgFolder.createSubfolder(feedProperties.feedName, msgWindow);
            folder = rootMsgFolder.FindSubFolder(feedProperties.feedName);
            var msgdb = folder.getMsgDatabase(null);
            msgdb.dBFolderInfo.setCharPtrProperty("feedUrl", feedProperties.feedLocation);
        }
        else if (new_folder) {
            // Do nothing, as everything is as it should be.
        }
        else {
            // Neither old nor new folders exist, so just create the new one.
            server.rootMsgFolder.createSubfolder(feedProperties.feedName, msgWindow);
            folder = rootMsgFolder.FindSubFolder(feedProperties.feedName);
            var msgdb = folder.getMsgDatabase(null);
            msgdb.dBFolderInfo.setCharPtrProperty("feedUrl", feedProperties.feedLocation);
        }
        updateTitle(item.id, feedProperties.feedName);
    }

    if (!feedProperties.feedLocation) {
        // The user cancelled the edit, but not until after potentially changing
        // the title, so despite the cancellation we should still redownload
        // the feed if the title has changed.
        if (new_title != old_title) {
            feed = new Feed(item.id);
            feed.download();
        }
        return;
    }
    else if (feedProperties.feedLocation != old_url)
        updateURL(item.id, feedProperties.feedLocation);

    feed = new Feed(item.id);
    feed.download();
}

function doRemove() {
    var tree = document.getElementById('subscriptions');
    var item = tree.view.getItemAtIndex(tree.view.selection.currentIndex);
    var resource = rdf.GetResource(item.id);
    var ds = getSubscriptionsDS();
    var feeds = getSubscriptionsList();
    var index = feeds.IndexOf(resource);
    if (index != -1) {
        var title = ds.GetTarget(resource, DC_TITLE, true);
        if (title) {
            title = title.QueryInterface(Components.interfaces.nsIRDFLiteral);
            // We could delete the folder, but what if other feeds are using it?
            // XXX Should we check for other feeds using the folder and delete it
            // if there aren't any?  What if the user is using the folder
            // for other purposes?
            var server = getIncomingServer();
            var openerResource = server.rootMsgFolder.QueryInterface(Components.interfaces.nsIRDFResource);
            var titleValue = title ? title.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : "";
            var feed = new Feed(resource);
            try {
              var folderResource = server.rootMsgFolder.getChildNamed(feed.name).QueryInterface(Components.interfaces.nsIRDFResource);
              var foo = window.opener.messenger.DeleteFolders(window.opener.GetFolderDatasource(), openerResource, folderResource);
            } catch (e) {}
            try {
                // If the folder still exists, then it wasn't deleted,
                // which means the user answered "no" to the question of whether
                // they wanted to move the folder into the trash.  That probably
                // means they changed their minds about removing the feed,
                // so don't remove it.
                folder = server.rootMsgFolder.getChildNamed(feed.name);
                if (folder) 
                  return;
            }
            catch (e) {}
            ds.Unassert(resource, DC_TITLE, title, true);
        }

        var url = ds.GetTarget(resource, DC_IDENTIFIER, true);
        if (url) {
            url = url.QueryInterface(Components.interfaces.nsIRDFLiteral);
            ds.Unassert(resource, DC_IDENTIFIER, url, true);
        }

        feeds.RemoveElementAt(index, true);
    }
    // Remove all assertions about the feed from the subscriptions database.
    removeAssertions(ds, feed);
    ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush(); // flush any changes

    // Remove all assertions about items in the feed from the items database.
    ds = getItemsDS();
    feed.invalidateItems();
    feed.removeInvalidItems();
    ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush(); // flush any changes
}


/*
 * Stuff
 */

function updateTitle(item, new_title) {
    var ds = getSubscriptionsDS();
    item = rdf.GetResource(item);
    var old_title = ds.GetTarget(item, DC_TITLE, true);
    if (old_title)
        ds.Change(item, DC_TITLE, old_title, rdf.GetLiteral(new_title));
    else
        ds.Assert(item, DC_TITLE, rdf.GetLiteral(new_title), true);
}

function updateURL(item, new_url) {
    var ds = getSubscriptionsDS();
    var item = rdf.GetResource(item);
    var old_url = ds.GetTarget(item, DC_IDENTIFIER, true);
    if (old_url)
        ds.Change(item, DC_IDENTIFIER, old_url, rdf.GetLiteral(new_url));
    else
        ds.Assert(item, DC_IDENTIFIER, rdf.GetLiteral(new_url), true);
}

function getIncomingServer() {
    return window.opener.getIncomingServer();
}

function getMessageWindow() {
  return window.opener.getMessageWindow();
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
