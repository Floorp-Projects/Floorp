/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * disttsc@bart.nl
 * jarrod.k.gray@rose-hulman.edu
 */

// cache these services
var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService().QueryInterface(Components.interfaces.nsIRDFService);
var dragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService().QueryInterface(Components.interfaces.nsIDragService);

var ctrlKeydown = false;
var gSrcCanRename;

function debugDump(msg)
{
  // uncomment for noise
  // dump(msg+"\n");
}

function DragOverTree(event)
{
    if (event.target.localName != "treecell" &&
        event.target.localName != "treeitem") {        
        event.preventBubble();
        return false;
    }

    var dragSession = null;
    var dragFolder = false;
    var flavor =false;

    dragSession = dragService.getCurrentSession();
    if ( !dragSession )	return(false);

    if ( dragSession.isDataFlavorSupported("text/nsmessageOrfolder") ) flavor = true;

    var treeItem = event.target.parentNode.parentNode;
    if (!treeItem)	return(false);

    var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
    if ( !trans ) return(false);

    var targetID = treeItem.getAttribute("id");
    var targetNode = RDF.GetResource(targetID, true);
    if (!targetNode)	return(false);
    var targetFolder = targetNode.QueryInterface(Components.interfaces.nsIMsgFolder);
    var targetServer = targetFolder.server;
    var sourceServer;

    trans.addDataFlavor("text/nsmessageOrfolder");
   
    for ( var i = 0; i < dragSession.numDropItems; ++i )
    {
       dragSession.getData ( trans, i );
       var dataObj = new Object();
       var bestFlavor = new Object();
       var len = new Object();
       try
       {
         trans.getAnyTransferData ( bestFlavor, dataObj, len );
       }
       catch (ex)
       {
          continue;   //no data so continue;
       }
       if ( dataObj )	dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
       if ( !dataObj )	continue;

       // pull the URL out of the data object
       var sourceID = dataObj.data.substring(0, len.value);
       if (!sourceID)	continue;

       var sourceNode;
       try
       {
         sourceNode = RDF.GetResource(sourceID, true);
         var folder = sourceNode.QueryInterface(Components.interfaces.nsIFolder);
         if (folder)
            dragFolder = true;
       }
       catch(ex)
       {
          sourceNode = null;
          var isServer = treeItem.getAttribute("IsServer");
          if (isServer == "true")
          {
     	    debugDump("***isServer == true\n");
     	    return(false);
          }
          var canFileMessages = treeItem.getAttribute("CanFileMessages");
          if (canFileMessages != "true")
          {
     	    debugDump("***canFileMessages == false\n");
     	    return(false);
          }
          var noSelect = treeItem.getAttribute("NoSelect");
          if (noSelect == "true")
          {
            debugDump("***NoSelect == true\n");
            return(false);
          } 
          var hdr = messenger.messageServiceFromURI(sourceID).messageURIToMsgHdr(sourceID);
          if (hdr.folder == targetFolder)
             return (false);
          break;
       }

       // we should only get here if we are dragging and dropping folders
       var sourceResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
  	   var sourceFolder = sourceResource.QueryInterface(Components.interfaces.nsIMsgFolder);
       sourceServer = sourceFolder.server;

       if (targetID == sourceID)	
          return (false);

      if (sourceServer != targetServer && targetServer.type == "imap") //don't allow drop on different imap servers.
          return (false);
			    
      if (targetFolder.URI == sourceFolder.parent.URI)   //don't allow immediate child to be dropped to it's parent
      {
          debugDump(targetFolder.URI + "\n");
          debugDump(sourceFolder.parent.URI + "\n");     
          return (false);
	  }
			
       var isAncestor = sourceFolder.isAncestorOf(targetFolder);
       if (isAncestor)  // don't allow parent to be dropped on its ancestors
          return (false);
	
    }

    if (dragFolder)
    {
       debugDump("***isFolderFlavor == true \n");  //first check these conditions then proceed further
        
       if (event.ctrlKey)   //ctrlkey does not apply to folder drag
          return(false);

       var canCreateSubfolders = treeItem.getAttribute('CanCreateSubfolders');
       if ( canCreateSubfolders == "false")  // if cannot create subfolders then a folder cannot be dropped here     
       {
          debugDump("***canCreateSubfolders == false \n");
          return(false);
       }
       var serverType = treeItem.getAttribute('ServerType');

       // if we've got a folder that can't be renamed
       // allow us to drop it if we plan on dropping it on "Local Folders"
       // (but not within the same server, to prevent renaming folders on "Local Folders" that
       // should not be renamed)
       if (gSrcCanRename == "false") {
          if (sourceServer == targetServer) {
            return(false);
          }
          if (serverType != "none") {
            return(false);
          }
	   }
    }

	//XXX other flavors here...

	// touch the attribute on the treeItem to trigger the repaint with the drop feedback
	// (recall that it is two levels above the target, which is a treeCell).
	//XXX this is really slow and likes to refresh N times per second.
    if (flavor) //message or folder
    {
     event.target.parentNode.parentNode.setAttribute ( "dd-triggerrepaint", 0 );
     dragSession.canDrop = true;
     event.preventBubble();  // do not propagate message
     return true;
    }
	
    return false;
}

function BeginDragTree(event, tree, flavor)
{
    if ( event.target == tree )
        return(true); // continue propagating the event
    
    var treeItem = event.target.parentNode.parentNode;
    if (!treeItem)	return(false);
        
    var childWithDatabase = tree;
    if ( ! childWithDatabase )
       return(false);

    var database = childWithDatabase.database;
    if ((!RDF) || (!database))	{ debugDump("CAN'T GET DATABASE\n"); return(false); }
	
    var dragStarted = false;

    var transArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
    if ( !transArray ) return(false); 

    var selArray = tree.selectedItems;
    var count = selArray.length;
    debugDump("selArray.length = " + count + "\n");
    for ( var i = 0; i < count; ++i )
    {

        var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
        if ( !trans )		return(false);

        var genTextData = Components.classes["@mozilla.org/supports-wstring;1"].createInstance(Components.interfaces.nsISupportsWString);
        if (!genTextData)	return(false);

        trans.addDataFlavor(flavor);
        
        // get id (url) 
        var id = selArray[i].getAttribute("id");
        genTextData.data = id;
        debugDump("    ID #" + i + " = " + id + "\n");

        trans.setTransferData ( flavor, genTextData, id.length * 2 );  // doublebyte byte data

        // put it into the transferable as an |nsISupports|
        var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
        transArray.AppendElement(genTrans);
    }

    var nsIDragService = Components.interfaces.nsIDragService;
    dragService.invokeDragSession ( event.target, transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
    nsIDragService.DRAGDROP_ACTION_MOVE );
    
    dragStarted = true;

    return(!dragStarted);  // don't propagate the event if a drag has begun
}

function BeginDragFolderTree(event)
{
    debugDump("BeginDragFolderTree\n");
    if (event.target.localName != "treecell" &&
        event.target.localName != "treeitem")
           return false;

    var tree = GetFolderTree();

    var treeItem = event.target.parentNode.parentNode;
    if (!treeItem)	return(false);

    gSrcCanRename = treeItem.getAttribute('CanRename');  //used in DragOverTree

    var serverType = treeItem.getAttribute('ServerType') // do not allow the drag when news is the source
    if ( serverType == "nntp") 
    {
      debugDump("***serverType == nntp \n");
      return(false);
	}

    return BeginDragTree(event, tree, "text/nsmessageOrfolder");

}

function DropOnFolderTree(event)
{
    debugDump("DropOnTree\n");

    var treeRoot = GetFolderTree();
    if (!treeRoot)	return(false);
    var treeDatabase = treeRoot.database;
    if (!treeDatabase)	return(false);

    // target is the <treecell>, and "id" is on the <treeitem> two levels above
    var treeItem = event.target.parentNode.parentNode;
    if (!treeItem)	return(false);

    if (event.ctrlKey)
        ctrlKeydown = true;
	else
        ctrlKeydown = false;
	// drop action is always "on" not "before" or "after"
	// get drop hint attributes
    var dropBefore = treeItem.getAttribute("dd-droplocation");
    var dropOn = treeItem.getAttribute("dd-dropon");

    var dropAction;
    if (dropOn == "true") 
        dropAction = "on";
    else
        return(false);

    var targetID = treeItem.getAttribute("id");
    if (!targetID)	return(false);

    debugDump("***targetID = " + targetID + "\n");

    //make sure target is a folder
    var dragSession = dragService.getCurrentSession();
    if ( !dragSession )	return(false);

    var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
    if ( !trans ) return(false);

    var list = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);

    var dropMessage = true;   	
    var sourceNode;
    var sourceFolder;
    var sourceServer;

    trans.addDataFlavor("text/nsmessageOrfolder");
	
    for ( var i = 0; i < dragSession.numDropItems; ++i )
    {
        dragSession.getData ( trans, i );
        var dataObj = new Object();
        var bestFlavor = new Object();
        var len = new Object();
        trans.getAnyTransferData ( bestFlavor, dataObj, len );
        if ( dataObj )	dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
        if ( !dataObj )	continue;

        // pull the URL out of the data object
        var sourceID = dataObj.data.substring(0, len.value);
        if (!sourceID)	continue;

        debugDump("    Node #" + i + ": drop '" + sourceID + "' " + dropAction + " '" + targetID + "'\n");

        sourceNode = RDF.GetResource(sourceID, true);
        // only do this for the first object, either they are all messages or they are all folders
        if (i == 0) {
            try {
                sourceFolder = sourceNode.QueryInterface(Components.interfaces.nsIMsgFolder);
                if (sourceFolder) {
                    // we are dropping a folder
                    dropMessage = false;
                }
                else {
                    dropMessage = true;
                }
            }
            catch (ex) {
                dropMessage = true;
            }
        }
        else {
            if (!dropMessage) {
                dump("drag and drop of multiple folders isn't supported\n");
            }
        }

        if (dropMessage) {
          // from the message uri, get the appropriate messenger service
          // and then from that service, get the msgDbHdr
          list.AppendElement(messenger.messageServiceFromURI(sourceID).messageURIToMsgHdr(sourceID));
        }
        else {
		  // Prevent dropping of a node before, after, or on itself
          if (sourceNode == targetNode)	
            continue;

          list.AppendElement(sourceNode);
        }
    }

    if (list.Count() < 1)
        return false;

    var isSourceNews = false;
    isSourceNews = isNewsURI(sourceID);
    
    var targetNode = RDF.GetResource(targetID, true);
    if (!targetNode)	return(false);
    var targetFolder = targetNode.QueryInterface(Components.interfaces.nsIMsgFolder);
    var targetServer = targetFolder.server;

    if (dropMessage) {
        // fix this, to get the folder from the sourceID.  this won't work with multiple 3 panes
        sourceFolder = GetThreadPaneFolder();
        sourceServer = sourceFolder.server;

        try {
          if (isSourceNews) {
            // news to pop or imap is always a copy
            messenger.CopyMessages(sourceFolder, targetFolder, list, false);
          }
          else {
            // fix this, will not work for multiple 3 panes
            if (!ctrlKeydown) {
              SetNextMessageAfterDelete();
            }
            messenger.CopyMessages(sourceFolder, targetFolder, list, !ctrlKeydown);
          }
        }
        catch (ex) {
          dump("failed to copy messages: " + ex + "\n");
        }
    }
    else {
        sourceServer = sourceFolder.server;
        try 
        {
           messenger.CopyFolders(treeDatabase,targetNode,list,(sourceServer == targetServer));
        }
        catch(ex)
        {
           dump ("Exception : CopyFolder " + ex + "\n");
        }
    }

    return(false);
}

function DropOnThreadPane(event)
{
	debugDump("DropOnThreadTree\n");
    /* you can't drop on the thread pane */
	return false;
}

function BeginDragThreadPane(event)
{
	debugDump("BeginDragThreadPane\n");
    debugDump("event.target.localName = " + event.target.localName + "\n");
    if (event.target.localName != "outlinerbody") return false;

    var outliner = GetThreadOutliner();

	return BeginDragOutliner(event, outliner, "text/nsmessageOrfolder");
}

function BeginDragOutliner(event, outliner, flavor)
{
    if (event.target == outliner) {
		return(true); // continue propagating the event
    }

    if (!outliner) {
        return(false);
    }

    var dragStarted = false;

    var transArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
    if ( !transArray ) return(false);

    var selArray = GetSelectedMessages();
    var count = selArray.length;
    debugDump("selArray.length = " + count + "\n");
    for ( var i = 0; i < count; ++i ) {
        var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
        if (!trans) return(false);

        var genTextData = Components.classes["@mozilla.org/supports-wstring;1"].createInstance(Components.interfaces.nsISupportsWString);
        if (!genTextData) return(false);

        trans.addDataFlavor(flavor);

        // get id (url)
        var id = selArray[i];
        genTextData.data = id;
        debugDump("    ID #" + i + " = " + id + "\n");

        trans.setTransferData ( flavor, genTextData, id.length * 2 );  // doublebyte byte data

        // put it into the transferable as an |nsISupports|
        var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
        transArray.AppendElement(genTrans);
    }

    var nsIDragService = Components.interfaces.nsIDragService;
    dragService.invokeDragSession ( event.target, transArray, null, nsIDragService.DRAGDROP_ACTION_COPY +
    nsIDragService.DRAGDROP_ACTION_MOVE );

    dragStarted = true;

    return(!dragStarted);  // don't propagate the event if a drag has begun
}
