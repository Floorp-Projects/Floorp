/*
 * Global Constants and Variables
 */

// forumzilla.js gets loaded by the main mail window, which provides this,
// but this script runs in its own window, which doesn't, so we need
// to provide it ourselves.
var accountManager =
  Components
    .classes["@mozilla.org/messenger/account-manager;1"]
      .getService();

/*
 * Event Handlers
 */

function doLoad() {
    // Display the list of feed subscriptions.
    var file = getSubscriptionsFile();
    var ds = getSubscriptionsDS();
    var tree = document.getElementById('subscriptions');
    tree.database.AddDataSource(ds);
    tree.builder.rebuild();
}

function doAdd() {
    var url = window.prompt("Location:", "");
    if (url == null)
        return;

    const DEFAULT_FEED_TITLE = "feed title";
    const DEFAULT_FEED_URL = "feed location";

    feed = new Feed(url || DEFAULT_FEED_URL);
    feed.download(false, false);
    if (!feed.title)
        feed.title = DEFAULT_FEED_TITLE;


    var server = getIncomingServer();
    var folder;
    try {
        //var folder = server.rootMsgFolder.FindSubFolder(feed.name);
        folder = server.rootMsgFolder.getChildNamed(feed.name);
    }
    catch(e) {
        // If we're here, it's probably because the folder doesn't exist yet,
        // so create it.
        debug("folder for new feed " + feed.title + " doesn't exist; creating");
        server.rootMsgFolder.createSubfolder(feed.name, getMessageWindow());
				folder = server.rootMsgFolder.FindSubFolder(feed.name);
				var msgdb = folder.getMsgDatabase(null);
				var folderInfo = msgdb.dBFolderInfo;
				folderInfo.setCharPtrProperty("feedUrl", feed.url);
    }

    // XXX This should be something like "subscribe to feed".
		dump ("feed name = " + feed.name + "\n");
    addFeed(feed.url, feed.title, null, folder);
    // XXX Maybe we can combine this with the earlier download?
    feed.download();
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

    new_title = window.prompt("Title:", old_title);
    if (new_title == null)
        return;
    else if (new_title != old_title) {
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
            new_folder = server.rootMsgFolder.getChildNamed(new_title);
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
            server.rootMsgFolder.createSubfolder(new_title, msgWindow);
        }
        else if (new_folder) {
            // Do nothing, as everything is as it should be.
        }
        else {
            // Neither old nor new folders exist, so just create the new one.
            server.rootMsgFolder.createSubfolder(new_title, msgWindow);
        }
        updateTitle(item.id, new_title);
    }

    new_url = window.prompt("Location:", old_url);
    if (new_url == null) {
        // The user cancelled the edit, but not until after potentially changing
        // the title, so despite the cancellation we should still redownload
        // the feed if the title has changed.
        if (new_title != old_title) {
            feed = new Feed(old_url, null, new_title);
            feed.download();
        }
        return;
    }
    else if (new_url != old_url)
        updateURL(item.id, new_url);

    feed = new Feed(new_url, null, new_title);
    feed.download();

    //tree.builder.rebuild();
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
            //var server = getIncomingServer();
            //var openerResource = server.rootMsgFolder.QueryInterface(Components.interfaces.nsIRDFResource);
            //var folderResource = server.rootMsgFolder.getChildNamed(title.Value).QueryInterface(Components.interfaces.nsIRDFResource);
            //var foo = window.opener.messenger.DeleteFolders(window.opener.GetFolderDatasource(), openerResource, folderResource);
            //try {
            //    // If the folder still exists, then it wasn't deleted,
            //    // which means the user answered "no" to the question of whether
            //    // they wanted to move the folder into the trash.  That probably
            //    // means they changed their minds about removing the feed,
            //    // so don't remove it.
            //    folder = server.rootMsgFolder.getChildNamed(feed.name);
            //    if (folder) return;
            //}
            //catch (e) {}
            ds.Unassert(resource, DC_TITLE, title, true);
        }

        var url = ds.GetTarget(resource, DC_IDENTIFIER, true);
        if (url) {
            url = url.QueryInterface(Components.interfaces.nsIRDFLiteral);
            ds.Unassert(resource, DC_IDENTIFIER, url, true);
        }

        feeds.RemoveElementAt(index, true);
    }
    //tree.builder.rebuild();
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
